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
#define HYSCAN_CACHE_RPC_STATUS_OK             (1)
#define HYSCAN_CACHE_RPC_STATUS_FAIL           (0)

enum
{
  HYSCAN_CACHE_RPC_PROC_VERSION = URPC_PROC_USER,
  HYSCAN_CACHE_RPC_PROC_SET,
  HYSCAN_CACHE_RPC_PROC_GET
};

enum
{
  HYSCAN_CACHE_RPC_PARAM_VERSION = URPC_PARAM_USER,
  HYSCAN_CACHE_RPC_PARAM_STATUS,
  HYSCAN_CACHE_RPC_PARAM_KEY,
  HYSCAN_CACHE_RPC_PARAM_DETAIL,
  HYSCAN_CACHE_RPC_PARAM_DATA
};

#endif /* __HYSCAN_CACHE_RPC_H__ */
