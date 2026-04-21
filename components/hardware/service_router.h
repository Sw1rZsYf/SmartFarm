/*
 * service_router.h - OneNET 服务路由层
 */
#ifndef SERVICE_ROUTER_H
#define SERVICE_ROUTER_H

#include "cJSON.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef void (*service_handler_t)(const cJSON *params, const char *msg_id, const char *service_id);

    void service_router_init(void);
    void service_router_register_builtin_services(void);
    int service_router_dispatch(const char *service_id, const cJSON *params, const char *msg_id);

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_ROUTER_H */