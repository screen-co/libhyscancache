/**
 * \file hyscan-cache-rpc.h
 *
 * \brief Файл констант RPC сервера системы кэширования данных
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#ifndef __HYSCAN_CACHE_RPC_H__
#define __HYSCAN_CACHE_RPC_H__

#include <urpc-types.h>

#define HYSCAN_CACHE_RPC_VERSION               (20151200)

#define HYSCAN_CACHE_RPC_PROC_VERSION          (URPC_PROC_USER + 1)
#define HYSCAN_CACHE_RPC_PROC_SET              (URPC_PROC_USER + 2)
#define HYSCAN_CACHE_RPC_PROC_GET              (URPC_PROC_USER + 3)
#define HYSCAN_CACHE_RPC_PROC_SIZE             (URPC_PROC_USER + 4)

#define HYSCAN_CACHE_RPC_PARAM_VERSION         (URPC_PARAM_USER + 1)
#define HYSCAN_CACHE_RPC_PARAM_STATUS          (URPC_PARAM_USER + 2)
#define HYSCAN_CACHE_RPC_PARAM_KEY             (URPC_PARAM_USER + 3)
#define HYSCAN_CACHE_RPC_PARAM_DETAIL          (URPC_PARAM_USER + 4)
#define HYSCAN_CACHE_RPC_PARAM_SIZE1           (URPC_PARAM_USER + 5)
#define HYSCAN_CACHE_RPC_PARAM_SIZE2           (URPC_PARAM_USER + 6)
#define HYSCAN_CACHE_RPC_PARAM_DATA1           (URPC_PARAM_USER + 7)
#define HYSCAN_CACHE_RPC_PARAM_DATA2           (URPC_PARAM_USER + 8)

#define HYSCAN_CACHE_RPC_STATUS_OK             (1)
#define HYSCAN_CACHE_RPC_STATUS_FAIL           (0)

#endif /* __HYSCAN_CACHE_RPC_H__ */
