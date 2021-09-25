#include "vice.h"

#include <errno.h>
#include <string.h>
#include <sys/poll.h>

#include "uiserver.h"

#include "archdep.h"
#include "lib.h"
#include "log.h"
#include "vicesocket.h"

/*
 * DESIGN THOUGHTS.
 *
 * VICE will launch a single UI child process. The UI can choose to
 * launch a further child UI process for handling two screens concurrently.
 * The UI server supports multiple UI processes connected to the same screen.
 */

/* Represents a connected uiclient */
typedef struct client_s {
    int id;
    vice_network_socket_t *socket;
} client_t;

/* Represents a single video output, such as VICII or VDC. */
typedef struct screen_s {
    video_canvas_t *canvas;
    client_t *clients;
} screen_t;

static vice_network_socket_t *server_socket;
static screen_t *screens;
static int screen_count;
static client_t *clients;
static int clients_size;
static int client_count;
static int next_client_id;

static struct pollfd *poll_fds;
static int poll_fd_count;
static bool rebuild_poll_fds_next_poll = true;

static void rebuild_poll_fds(void);

int uiserver_init(void)
{
    int retval = 0;
    
    vice_network_socket_address_t *address;
    
    /*
     * We let the OS choose the listen port so multiple emu
     * can run at the same time. The port is passed to the UI
     * process when launched.
     *
     * If a use case is found for running the emulation on one
     * machine and the ui on another (old ARM devices?) then it
     * will be straightforward to bind to 0.0.0.0 or take a
     * specific address string from a resource.
     */
    
    address = vice_network_address_generate("ip4://127.0.0.1", 0);
    
    server_socket = vice_network_server("UI server", address);
    if (!server_socket) {
        retval = -1;
        goto done;
    }
    
done:
    vice_network_address_close(address);
    return retval;
}

void uiserver_add_screen(video_canvas_t *canvas)
{
    int i;
    int new_screen_index;

    /*
     * Called when a video chip first initialises. The uiserver
     * makes this screen available for uiclients to subscribe to.
     */

    for (i = 0; i < screen_count; i++) {
        if (screens[i].canvas == canvas) {
            log_error(LOG_DEFAULT, "UI Server: Attempt to re-add screen for %s", canvas->videoconfig->chip_name);
            return;
        }
    }

    log_message(LOG_DEFAULT, "UI Server: Adding screen for %s", canvas->videoconfig->chip_name);
    
    new_screen_index = screen_count;
    screen_count++;

    screens = lib_realloc(screens, screen_count * sizeof(screen_t));

    screens[new_screen_index].canvas = canvas;
}

void uiserver_await_ready(void)
{

}

static void handle_new_client(void)
{
    vice_network_socket_t *client_socket;
    int client_index;
    
    client_socket = vice_network_accept(server_socket);
    client_index = client_count++;
    
    if (client_count > clients_size) {
        clients = lib_realloc(clients, client_count * sizeof(client_t));
        clients_size = client_count;
    }
    
    clients[client_index].id     = next_client_id++;
    clients[client_index].socket = client_socket;

    log_message(LOG_DEFAULT, "New client %d connected", clients[client_index].id);
    
    rebuild_poll_fds_next_poll = true;
    
    // HACK
    vice_network_send(client_socket, "hello\n", 6, 0);
}

static void handle_client_error(int client_index)
{
    int i;
    
    log_message(LOG_DEFAULT, "Socket error on client %d", clients[client_index].id);
    
    vice_network_socket_close(clients[client_index].socket);
    clients[client_index].socket = NULL;

    /* remove the client from the array, shift newer clients back an index */
    for (i = client_index; i < client_count - 1; i++) {
        clients[i] = clients[i + 1];
    }
    memset(&clients[i], 0, sizeof(client_t));
    
    client_count--;
    
    rebuild_poll_fds_next_poll = true;
}

/** \brief Handle all uiclient IO */
void uiserver_poll(void)
{
    int i;
    int read_fd_count;
    
    /*
     * Rebuild our pollfd array if needed
     */
    
    if (rebuild_poll_fds_next_poll) {
        rebuild_poll_fds();
        rebuild_poll_fds_next_poll = false;
    }

    /*
     * Do any of our sockets have any pending events?
     */

    read_fd_count = poll(poll_fds, poll_fd_count, 0);

    if (read_fd_count == 0) {
        /* No. */
        return;
    }

    if (read_fd_count == -1) {
        log_error(LOG_DEFAULT, "UI Server: poll error: %s", strerror(errno));
        archdep_vice_exit(1);
    }

    /* Something is waiting for us. Check client IO first. */
    for (i = 0; i < client_count; i++) {
        if (poll_fds[i].revents == POLLIN) {
            log_message(LOG_DEFAULT, "POLLIN on client %d", clients[i].id);
            // HACK
            char b[5 + 1];
            int r;
            r = vice_network_receive(clients[i].socket, b, 5, 0);
            if (r == -1) {
                handle_client_error(i);
            } else {
                b[r] = '\0';
                log_message(LOG_DEFAULT, "Client %d says [%s]", clients[i].id, b);
            }
        } else if (poll_fds[i].revents) {
            /* Any other event is an error */
            handle_client_error(i);
        }
    }

    /* Last, check for new client connections */
    if (poll_fds[i].revents == POLLIN) {
        handle_new_client();
    } else if (poll_fds[i].revents) {
        log_error(LOG_DEFAULT, "UI Server: server socket error");
        archdep_vice_exit(1);
    }
}

void uiserver_shutdown(void)
{
    int i;
    
    /* Notifiy all connected clients that we're shutting down */
    for (i = 0; i < client_count; i++) {
        vice_network_socket_close(clients[i].socket);
        clients[i].socket = NULL;
    }
    
    lib_free(clients);
    clients = NULL;
    clients_size = 0;
    client_count = 0;
    
    if (server_socket) {
        vice_network_socket_close(server_socket);
        server_socket = NULL;
    }
    
    lib_free(poll_fds);
    poll_fds = NULL;
    
    lib_free(screens);
    screens = NULL;
    screen_count = 0;
}

/************/

static void rebuild_poll_fds(void)
{
    int i;

    lib_free(poll_fds);
    poll_fds = NULL;

    poll_fd_count = client_count + 1;
    poll_fds = lib_calloc(poll_fd_count, sizeof(struct pollfd));

    /* Add all client sockets */
    for (i = 0; i < client_count; i++) {
        poll_fds[i].fd = vice_network_get_socket(clients[i].socket);
        poll_fds[i].events = POLLIN;
    }

    /* Add the server socket to the end */
    poll_fds[i].fd = vice_network_get_socket(server_socket);
    poll_fds[i].events = POLLIN;
}
