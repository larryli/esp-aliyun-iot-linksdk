/* 注意：core_sysdep_network_xxx 系列依赖函数是基于posix兼容的网络api实现的，用户应根据实际的网络接口类型进行重新适配 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <errno.h>
#include "aiot_state_api.h"
#include "aiot_sysdep_api.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

typedef struct {
    int fd;
    core_sysdep_socket_type_t socket_type;
    char *host;
    uint16_t port;
    uint32_t connect_timeout_ms;
} core_network_handle_t;

void *core_sysdep_malloc(uint32_t size, char *name)
{
    return pvPortMalloc(size);
}

void core_sysdep_free(void *ptr)
{
    vPortFree(ptr);
}

uint64_t core_sysdep_time(void)
{
    return (uint64_t)(xTaskGetTickCount() * portTICK_RATE_MS);
}

void core_sysdep_sleep(uint64_t time_ms)
{
    vTaskDelay(pdMS_TO_TICKS(time_ms));
}

void *core_sysdep_network_init(void)
{
    core_network_handle_t *handle = NULL;

    handle = pvPortMalloc(sizeof(core_network_handle_t));
    if (handle == NULL) {
        return NULL;
    }
    memset(handle, 0, sizeof(core_network_handle_t));

    return handle;
}

int32_t core_sysdep_network_setopt(void *handle, core_sysdep_network_option_t option, void *data)
{
    core_network_handle_t *network_handle = (core_network_handle_t *)handle;

    if (handle == NULL || data == NULL) {
        return STATE_PORT_INPUT_NULL_POINTER;
    }

    if (option >= CORE_SYSDEP_NETWORK_MAX) {
        return STATE_PORT_INPUT_OUT_RANGE;
    }

    switch (option) {
        case CORE_SYSDEP_NETWORK_SOCKET_TYPE: {
            network_handle->socket_type = *(core_sysdep_socket_type_t *)data;
        }
        break;
        case CORE_SYSDEP_NETWORK_HOST: {
            network_handle->host = pvPortMalloc(strlen(data) + 1);
            if (network_handle->host == NULL) {
                printf("malloc failed\n");
                return STATE_PORT_MALLOC_FAILED;
            }
            memset(network_handle->host, 0, strlen(data) + 1);
            memcpy(network_handle->host, data, strlen(data));
        }
        break;
        case CORE_SYSDEP_NETWORK_PORT: {
            network_handle->port = *(uint16_t *)data;
        }
        break;
        case CORE_SYSDEP_NETWORK_CONNECT_TIMEOUT_MS: {
            network_handle->connect_timeout_ms = *(uint32_t *)data;
        }
        break;
        default: {
            printf("unknown option\n");
        }
    }

    return STATE_SUCCESS;
}

static int32_t _core_sysdep_network_tcp_establish(core_network_handle_t *network_handle)
{
    int32_t res = STATE_SUCCESS, fd = 0;
    char service[6] = {0};
    struct addrinfo hints;
    struct addrinfo *addrInfoList = NULL, *pos = NULL;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; /* only IPv4 */
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = 0;
    sprintf(service, "%u", network_handle->port);

    printf("establish tcp connection with server(host='%s', port=[%u])\n", network_handle->host, network_handle->port);

    res = getaddrinfo(network_handle->host, service, &hints, &addrInfoList);
    if (res != 0) {
        printf("getaddrinfo error, errno: %d, host: %s, port: %s\n", errno, network_handle->host, service);
        return STATE_PORT_NETWORK_DNS_FAILED;
    }

    for (pos = addrInfoList; pos != NULL; pos = pos->ai_next) {
        if (pos->ai_family != AF_INET) {
            printf("socket type error\n");
            res = -1;
            continue;
        }

        fd = socket(pos->ai_family, pos->ai_socktype, pos->ai_protocol);
        if (fd < 0) {
            printf("create socket error\n");
            res = -1;
            continue;
        }

        if (connect(fd, pos->ai_addr, pos->ai_addrlen) == 0) {
            network_handle->fd = fd;
            res = STATE_SUCCESS;
            break;
        }

        close(fd);
        printf("connect error, errno: %d\n", errno);
    }

    if (res < 0) {
        printf("fail to establish tcp\n");
        res = STATE_PORT_NETWORK_CONNECT_FAILED;
    } else {
        printf("success to establish tcp, fd=%d\n", network_handle->fd);
        res = STATE_SUCCESS;
    }
    freeaddrinfo(addrInfoList);

    return res;
}

int32_t core_sysdep_network_establish(void *handle)
{
    core_network_handle_t *network_handle = (core_network_handle_t *)handle;

    if (handle == NULL) {
        return STATE_PORT_INPUT_NULL_POINTER;
    }

    if (network_handle->socket_type == CORE_SYSDEP_SOCKET_TCP_CLIENT) {
        if (network_handle->host == NULL) {
            return STATE_PORT_MISSING_HOST;
        }
        return _core_sysdep_network_tcp_establish(network_handle);
    } else if (network_handle->socket_type == CORE_SYSDEP_SOCKET_TCP_SERVER) {
        return STATE_PORT_TCP_SERVER_NOT_IMPLEMENT;
    } else if (network_handle->socket_type == CORE_SYSDEP_SOCKET_UDP_CLIENT) {
        return  _core_sysdep_network_connect(network_handle->host, network_handle->port,
                                             AF_UNSPEC, SOCK_DGRAM, IPPROTO_UDP, network_handle->connect_timeout_ms, &network_handle->fd);
    } else if (network_handle->socket_type == CORE_SYSDEP_SOCKET_UDP_SERVER) {
        return STATE_PORT_UDP_SERVER_NOT_IMPLEMENT;
    }

    printf("unknown nwk type or tcp host absent\n");

    return STATE_PORT_NETWORK_UNKNOWN_SOCKET_TYPE;
}

static int32_t _core_sysdep_network_recv(core_network_handle_t *network_handle, uint8_t *buffer, uint32_t len,
        uint32_t timeout_ms)
{
    int res = 0;
    int32_t recv_bytes = 0;
    ssize_t recv_res = 0;
    uint64_t timestart_ms = 0, timenow_ms = 0, timeselect_ms = 0;
    fd_set recv_sets;
    struct timeval timeselect;

    FD_ZERO(&recv_sets);
    FD_SET(network_handle->fd, &recv_sets);

    /* Start Time */
    timestart_ms = core_sysdep_time();
    timenow_ms = timestart_ms;

    do {
        timenow_ms = core_sysdep_time();

        if (timenow_ms - timestart_ms >= timenow_ms ||
            timeout_ms - (timenow_ms - timestart_ms) > timeout_ms) {
            break;
        }

        timeselect_ms = timeout_ms - (timenow_ms - timestart_ms);
        timeselect.tv_sec = timeselect_ms / 1000;
        timeselect.tv_usec = timeselect_ms % 1000 * 1000;

        res = select(network_handle->fd + 1, &recv_sets, NULL, NULL, &timeselect);
        if (res == 0) {
            /* printf("_core_sysdep_network_recv, nwk select timeout\n"); */
            continue;
        } else if (res < 0) {
            printf("_core_sysdep_network_recv, errno: %d\n", errno);
            perror("_core_sysdep_network_recv, nwk select failed: ");
            return STATE_PORT_NETWORK_SELECT_FAILED;
        } else {
            if (FD_ISSET(network_handle->fd, &recv_sets)) {
                recv_res = recv(network_handle->fd, buffer + recv_bytes, len - recv_bytes, 0);
                if (recv_res == 0) {
                    printf("_core_sysdep_network_recv, nwk connection closed\n");
                    return STATE_PORT_NETWORK_RECV_CONNECTION_CLOSED;
                } else if (recv_res < 0) {
                    printf("_core_sysdep_network_recv, errno: %d\n", errno);
                    perror("_core_sysdep_network_recv, nwk recv error: ");
                    if (errno == EINTR) {
                        continue;
                    }
                    return STATE_PORT_NETWORK_RECV_FAILED;
                } else {
                    recv_bytes += recv_res;
                    /* printf("recv_bytes: %d, len: %d\n",recv_bytes,len); */
                    if (recv_bytes == len) {
                        break;
                    }
                }
            }
        }
    } while (((timenow_ms - timestart_ms) < timeout_ms) && (recv_bytes < len));

    /* printf("%s: recv over\n",__FUNCTION__); */
    return recv_bytes;
}

int32_t core_sysdep_network_recv(void *handle, uint8_t *buffer, uint32_t len, uint32_t timeout_ms,
                                 core_sysdep_addr_t *addr)
{
    core_network_handle_t *network_handle = (core_network_handle_t *)handle;

    if (handle == NULL || buffer == NULL) {
        return STATE_PORT_INPUT_NULL_POINTER;
    }

    if (len == 0 || timeout_ms == 0) {
        return STATE_PORT_INPUT_OUT_RANGE;
    }

    if (network_handle->socket_type == CORE_SYSDEP_SOCKET_TCP_CLIENT) {
        return _core_sysdep_network_recv(network_handle, buffer, len, timeout_ms);
    } else if (network_handle->socket_type == CORE_SYSDEP_SOCKET_TCP_SERVER) {
        return STATE_PORT_TCP_SERVER_NOT_IMPLEMENT;
    } else if (network_handle->socket_type == CORE_SYSDEP_SOCKET_UDP_CLIENT) {
        return _core_sysdep_network_recv(network_handle, buffer, len, timeout_ms);
    } else if (network_handle->socket_type == CORE_SYSDEP_SOCKET_UDP_SERVER) {
        return STATE_PORT_UDP_SERVER_NOT_IMPLEMENT;
    }

    printf("unknown nwk type\n");

    return STATE_PORT_NETWORK_UNKNOWN_SOCKET_TYPE;
}

int32_t _core_sysdep_network_send(core_network_handle_t *network_handle, uint8_t *buffer, uint32_t len,
                                  uint32_t timeout_ms)
{
    int res = 0;
    int32_t send_bytes = 0;
    ssize_t send_res = 0;
    uint64_t timestart_ms = 0, timenow_ms = 0, timeselect_ms = 0;
    fd_set send_sets;
    struct timeval timeselect;

    FD_ZERO(&send_sets);
    FD_SET(network_handle->fd, &send_sets);

    /* Start Time */
    timestart_ms = core_sysdep_time();
    timenow_ms = timestart_ms;

    do {
        timenow_ms = core_sysdep_time();

        if (timenow_ms - timestart_ms >= timenow_ms ||
            timeout_ms - (timenow_ms - timestart_ms) > timeout_ms) {
            break;
        }

        timeselect_ms = timeout_ms - (timenow_ms - timestart_ms);
        timeselect.tv_sec = timeselect_ms / 1000;
        timeselect.tv_usec = timeselect_ms % 1000 * 1000;

        res = select(network_handle->fd + 1, NULL, &send_sets, NULL, &timeselect);
        if (res == 0) {
            printf("_core_sysdep_network_send, nwk select timeout\n");
            continue;
        } else if (res < 0) {
            printf("_core_sysdep_network_send, errno: %d\n", errno);
            perror("_core_sysdep_network_send, nwk select failed: ");
            return STATE_PORT_NETWORK_SELECT_FAILED;
        } else {
            if (FD_ISSET(network_handle->fd, &send_sets)) {
                send_res = send(network_handle->fd, buffer + send_bytes, len - send_bytes, 0);
                if (send_res == 0) {
                    printf("_core_sysdep_network_send, nwk connection closed\n");
                    return STATE_PORT_NETWORK_SEND_CONNECTION_CLOSED;
                } else if (send_res < 0) {
                    printf("_core_sysdep_network_send, errno: %d\n", errno);
                    perror("_core_sysdep_network_send, nwk recv error: ");
                    if (errno == EINTR) {
                        continue;
                    }
                    return STATE_PORT_NETWORK_SEND_FAILED;
                } else {
                    send_bytes += send_res;
                    if (send_bytes == len) {
                        break;
                    }
                }
            }
        }
    } while (((timenow_ms - timestart_ms) < timeout_ms) && (send_bytes < len));

    return send_bytes;
}
int32_t core_sysdep_network_send(void *handle, uint8_t *buffer, uint32_t len, uint32_t timeout_ms,
                                 core_sysdep_addr_t *addr)
{
    core_network_handle_t *network_handle = (core_network_handle_t *)handle;

    if (handle == NULL || buffer == NULL) {
        printf("invalid parameter\n");
        return STATE_PORT_INPUT_NULL_POINTER;
    }
    if (len == 0 || timeout_ms == 0) {
        return STATE_PORT_INPUT_OUT_RANGE;
    }

    if (network_handle->socket_type == CORE_SYSDEP_SOCKET_TCP_CLIENT) {
        return _core_sysdep_network_send(network_handle, buffer, len, timeout_ms);
    } else if (network_handle->socket_type == CORE_SYSDEP_SOCKET_TCP_SERVER) {
        return STATE_PORT_TCP_SERVER_NOT_IMPLEMENT;
    } else if (network_handle->socket_type == CORE_SYSDEP_SOCKET_UDP_CLIENT) {
        return _core_sysdep_network_send(network_handle, buffer, len, timeout_ms);
    } else if (network_handle->socket_type == CORE_SYSDEP_SOCKET_UDP_SERVER) {
        return STATE_PORT_UDP_SERVER_NOT_IMPLEMENT;
    }

    printf("unknown nwk type\n");

    return STATE_PORT_NETWORK_UNKNOWN_SOCKET_TYPE;
}

static void _core_sysdep_network_tcp_disconnect(core_network_handle_t *network_handle)
{
    shutdown(network_handle->fd, 2);
    close(network_handle->fd);
}

int32_t core_sysdep_network_deinit(void **handle)
{
    core_network_handle_t *network_handle = NULL;

    if (handle == NULL || *handle == NULL) {
        return STATE_PORT_INPUT_NULL_POINTER;
    }

    network_handle = *(core_network_handle_t **)handle;

    /* Shutdown both send and receive operations. */
    if (network_handle->socket_type == 0 && network_handle->host != NULL) {
        _core_sysdep_network_tcp_disconnect(network_handle);
    }

    if (network_handle->host != NULL) {
        vPortFree(network_handle->host);
        network_handle->host = NULL;
    }

    vPortFree(network_handle);
    *handle = NULL;

    return 0;
}

void core_sysdep_rand(uint8_t *output, uint32_t output_len)
{
    /* 使用idf提供的硬件随机数生成接口, 仅适用于esp8266和esp32 */
    esp_fill_random(output, output_len);
}

void *core_sysdep_mutex_init(void)
{
    return xSemaphoreCreateMutex();
}

void core_sysdep_mutex_lock(void *mutex)
{
    xSemaphoreTake((SemaphoreHandle_t)mutex, portMAX_DELAY);
}

void core_sysdep_mutex_unlock(void *mutex)
{
    xSemaphoreGive((SemaphoreHandle_t)mutex);
}

void core_sysdep_mutex_deinit(void **mutex)
{
    if (mutex != NULL || *mutex != NULL) {
        vSemaphoreDelete((SemaphoreHandle_t)*mutex);
        *mutex = NULL;
    }
}

aiot_sysdep_portfile_t g_aiot_sysdep_portfile = {
    core_sysdep_malloc,
    core_sysdep_free,
    core_sysdep_time,
    core_sysdep_sleep,
    core_sysdep_network_init,
    core_sysdep_network_setopt,
    core_sysdep_network_establish,
    core_sysdep_network_recv,
    core_sysdep_network_send,
    core_sysdep_network_deinit,
    core_sysdep_rand,
    core_sysdep_mutex_init,
    core_sysdep_mutex_lock,
    core_sysdep_mutex_unlock,
    core_sysdep_mutex_deinit
};
