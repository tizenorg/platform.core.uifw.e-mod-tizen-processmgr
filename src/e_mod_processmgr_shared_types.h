#ifdef E_TYPEDEFS
#else
#ifndef E_MOD_PROCESSMGR_SHARED_TYPES_H
#define E_MOD_PROCESSMGR_SHARED_TYPES_H

typedef struct _E_ProcessMgr  E_ProcessMgr;
typedef struct _E_ProcessInfo E_ProcessInfo;
typedef struct _E_WindowInfo  E_WindowInfo;

typedef enum _E_ProcessMgr_Action
{
   PROCESS_LAUNCH = 0,
   PROCESS_RESUME = 1,
   PROCESS_TERMINATE = 2,
   PROCESS_FOREGROUND = 3,
   PROCESS_BACKGROUND = 4
} E_ProcessMgr_Action;

typedef enum _E_Process_State
{
   PROCESS_STATE_UNKNOWN,
   PROCESS_STATE_BACKGROUND,
   PROCESS_STATE_FOREGROUND,
} E_Process_State;

struct _E_ProcessMgr
{
   Eina_Hash         *pids_hash;
   Eina_Hash         *wins_hash;
   Eina_Inlist       *pids_list;
   Eina_Inlist       *wins_list;
};

struct _E_ProcessInfo
{
   EINA_INLIST;
   int              pid;
   Eina_List       *wins;
   E_Process_State  state;
   Eina_Bool        launch;
};

struct _E_WindowInfo
{
   EINA_INLIST;
   E_Client      *win;
   pid_t          pid;
};

#endif
#endif
