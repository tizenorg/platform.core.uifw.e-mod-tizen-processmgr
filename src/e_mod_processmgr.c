#include "e.h"
#include "e_mod_processmgr.h"
#include "e_mod_processmgr_shared_types.h"


static E_ProcessMgr *_pm = NULL;

static Eina_List *handlers = NULL;
static Eina_List *hooks_ec = NULL;


static E_ProcessMgr *_e_processmgr_new(void);
static void _e_processmgr_del(E_ProcessMgr *pm);

static E_ProcessInfo *_e_processmgr_processinfo_find(E_ProcessMgr *pm, pid_t pid);
static Eina_Bool _e_processmgr_processinfo_add(pid_t pid, E_Client *ec);
static Eina_Bool _e_processmgr_processinfo_del(pid_t pid, E_Client *ec);

static E_WindowInfo *_e_processmgr_client_info_find(E_ProcessMgr *pm, E_Client *ec);
static Eina_Bool _e_processmgr_client_info_add(E_Client *ec);
static void _e_processmgr_client_info_del(E_Client *ec);

static Eina_Bool _e_processmgr_cb_client_add(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool _e_processmgr_cb_client_remove(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool _e_processmgr_cb_client_iconify(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool _e_processmgr_cb_client_uniconify(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool _e_processmgr_cb_client_visibility_change(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool _e_processmgr_cb_client_focus_in(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);

static void _pol_cb_hook_client_visibility(void *d EINA_UNUSED, E_Client *ec);

static Eina_Bool _e_processmgr_client_freeze_condition_check(E_Client *ec);
static Eina_Bool _e_processmgr_client_freeze(E_Client *ec);
static Eina_Bool _e_processmgr_client_thaw(E_Client *ec);

static void _e_processmgr_send_pid_action(int pid, E_ProcessMgr_Action act);


static E_ProcessMgr *
_e_processmgr_new(void)
{
   E_ProcessMgr *pm;

   pm = E_NEW(E_ProcessMgr, 1);
   if (!pm) return NULL;

   pm->pids_hash = eina_hash_pointer_new(NULL);
   if (!pm->pids_hash) goto error;
   pm->wins_hash = eina_hash_pointer_new(NULL);
   if (!pm->wins_hash) goto error;

   return pm;

error:
   if (pm->pids_hash)
     eina_hash_free(pm->pids_hash);

   E_FREE(pm);
   return NULL;

}

static void
_e_processmgr_del(E_ProcessMgr *pm)
{
   E_WindowInfo   *winfo = NULL;
   E_ProcessInfo  *pinfo = NULL;

   if (!pm) return;

   while (pm->wins_list)
     {
        winfo = (E_WindowInfo *)(pm->wins_list);
        eina_hash_del_by_key(pm->wins_hash, &winfo->win);
        //eina_hash_del(pm->wins_hash, &winfo->win, winfo);
        pm->wins_list = eina_inlist_remove(pm->wins_list, EINA_INLIST_GET(winfo));
        E_FREE(winfo);
     }

   while (pm->pids_list)
     {
        pinfo = (E_ProcessInfo *)(pm->pids_list);
        eina_hash_del_by_key(pm->pids_hash, &pinfo->pid);
        //eina_hash_del(pm->pids_hash, &pinfo->pid, pinfo);
        pm->pids_list = eina_inlist_remove(pm->pids_list, EINA_INLIST_GET(pinfo));
        E_FREE(pinfo);
     }

   if (pm->pids_hash)
     {
        eina_hash_free(pm->pids_hash);
        pm->pids_hash = NULL;
     }

   if (pm->wins_hash)
     {
        eina_hash_free(pm->wins_hash);
        pm->wins_hash = NULL;
     }

   E_FREE(pm);
}

static E_ProcessInfo *
_e_processmgr_processinfo_find(E_ProcessMgr *pm, pid_t pid)
{
   if (!pm) return NULL;
   return eina_hash_find(pm->pids_hash, &pid);
}

static Eina_Bool
_e_processmgr_processinfo_add(pid_t pid, E_Client *ec)
{
   E_ProcessInfo  *pinfo = NULL;

   if (pid <= 0) return EINA_FALSE;
   if (!ec) return EINA_FALSE;

   pinfo = _e_processmgr_processinfo_find(_pm, pid);
   if (!pinfo)
     {
        pinfo = E_NEW(E_ProcessInfo, 1);
        if (!pinfo) return EINA_FALSE;

        pinfo->pid = pid;
        pinfo->state = PROCESS_STATE_UNKNOWN;

        eina_hash_add(_pm->pids_hash, &pid, pinfo);
        _pm->pids_list = eina_inlist_append(_pm->pids_list, EINA_INLIST_GET(pinfo));
     }

   pinfo->wins = eina_list_append(pinfo->wins, ec);

   return EINA_TRUE;
}

static Eina_Bool
_e_processmgr_processinfo_del(pid_t pid, E_Client *ec)
{
   E_ProcessInfo *pinfo = NULL;
   Eina_List *l;
   E_Client *temp_ec;
   Eina_Bool found = EINA_FALSE;

   if (pid <= 0) return EINA_FALSE;
   if (!ec) return EINA_FALSE;

   pinfo = _e_processmgr_processinfo_find(_pm, pid);
   if (!pinfo) return EINA_FALSE;

   EINA_LIST_FOREACH(pinfo->wins, l, temp_ec)
     {
        if (temp_ec == ec)
          {
             found = EINA_TRUE;
             break;
          }
     }

   if (found)
     pinfo->wins = eina_list_remove(pinfo->wins, ec);

   if (!(pinfo->wins))
     {
        eina_hash_del_by_key(_pm->pids_hash, &pid);
        //eina_hash_del(_pm->pids_hash, &pid, pinfo);
        _pm->pids_list = eina_inlist_remove(_pm->pids_list, EINA_INLIST_GET(pinfo));
        E_FREE(pinfo);
     }

   return EINA_TRUE;
}


static E_WindowInfo *
_e_processmgr_client_info_find(E_ProcessMgr *pm, E_Client *ec)
{
   if (!pm) return NULL;
   return eina_hash_find(pm->wins_hash, &ec);
}

static Eina_Bool
_e_processmgr_client_info_add(E_Client *ec)
{
   E_WindowInfo *winfo = NULL;

   if (!ec) return EINA_FALSE;

   if (_e_processmgr_client_info_find(_pm, ec))
     return EINA_TRUE;

   winfo = E_NEW(E_WindowInfo, 1);
   if (!winfo) return EINA_FALSE;

   winfo->win = ec;
   winfo->pid = ec->netwm.pid;

   eina_hash_add(_pm->wins_hash, &ec, winfo);
   _pm->wins_list = eina_inlist_append(_pm->wins_list, EINA_INLIST_GET(winfo));

   _e_processmgr_processinfo_add(winfo->pid, ec);

   return EINA_TRUE;
}

static void
_e_processmgr_client_info_del(E_Client *ec)
{
   E_WindowInfo *winfo = NULL;

   if (!ec) return;

   winfo = _e_processmgr_client_info_find(_pm, ec);
   if (!winfo) return;

   if (_pm->active_win == ec)
     {
        _pm->active_win = NULL;
        ELOGF("PROCESSMGR STATE", "PROCESS_DEACTIVATE. PID:%d", NULL, NULL, winfo->pid);
        _e_processmgr_send_pid_action(winfo->pid, PROCESS_DEACTIVATE);
     }

   _e_processmgr_processinfo_del(winfo->pid, ec);

   eina_hash_del_by_key(_pm->wins_hash, &ec);
   //eina_hash_del(_pm->wins_hash, &ec, winfo);
   _pm->wins_list = eina_inlist_remove(_pm->wins_list, EINA_INLIST_GET(winfo));

   E_FREE(winfo);

   return;
}

static Eina_Bool
_e_processmgr_cb_client_add(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev;
   E_Client *ec;

   ev = event;
   if (!ev) return ECORE_CALLBACK_PASS_ON;

   ec = ev->ec;

   _e_processmgr_client_info_add(ec);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_processmgr_cb_client_remove(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev;
   E_Client *ec;

   ev = event;
   if (!ev) return ECORE_CALLBACK_PASS_ON;

   ec = ev->ec;
   _e_processmgr_client_info_del(ec);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_processmgr_cb_client_iconify(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev;
   E_Client *ec;

   ev = event;
   if (!ev) return ECORE_CALLBACK_PASS_ON;

   ec = ev->ec;

   // check all ECs of its pid, if yes, freeze
   if (_e_processmgr_client_freeze_condition_check(ec))
     _e_processmgr_client_freeze(ec);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_processmgr_cb_client_uniconify(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev;
   E_Client *ec;

   ev = event;
   if (!ev) return ECORE_CALLBACK_PASS_ON;

   ec = ev->ec;
   _e_processmgr_client_thaw(ec);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_processmgr_cb_client_visibility_change(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev;
   E_Client *ec;

   ev = event;
   if (!ev) return ECORE_CALLBACK_PASS_ON;

   ec = ev->ec;
   if (ec->visibility.obscured == E_VISIBILITY_UNOBSCURED)
     _e_processmgr_client_thaw(ec);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_processmgr_cb_client_focus_in(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev;
   E_Client *ec;
   E_Client *ec_deactive;
   E_WindowInfo *winfo = NULL;
   E_WindowInfo *winfo_deactive = NULL;
   E_ProcessInfo *pinfo  = NULL;
   Eina_Bool change_active = EINA_FALSE;

   ev = event;
   if (!ev) return ECORE_CALLBACK_PASS_ON;

   ec = ev->ec;

   winfo = _e_processmgr_client_info_find(_pm, ec);
   if (!winfo) return EINA_FALSE;

   if (winfo->pid <= 0) return EINA_FALSE;

   pinfo = _e_processmgr_processinfo_find(_pm, winfo->pid);
   if (!pinfo) return EINA_FALSE;

   ec_deactive = _pm->active_win;
   _pm->active_win = ec;

   winfo_deactive = _e_processmgr_client_info_find(_pm, ec_deactive);
   if (!winfo_deactive)
     {
        change_active = EINA_TRUE;
     }
   else
     {
        if (winfo_deactive->pid != winfo->pid)
          {
             change_active = EINA_TRUE;
          }
     }

   if (change_active)
     {
        ELOGF("PROCESSMGR STATE", "PROCESS_ACTIVATE. PID:%d", NULL, NULL, winfo->pid);
        _e_processmgr_send_pid_action(winfo->pid, PROCESS_ACTIVATE);

        if (winfo_deactive)
          {
             ELOGF("PROCESSMGR STATE", "PROCESS_DEACTIVATE. PID:%d", NULL, NULL, winfo_deactive->pid);
             _e_processmgr_send_pid_action(winfo->pid, PROCESS_DEACTIVATE);
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static void
_pol_cb_hook_client_visibility(void *d EINA_UNUSED, E_Client *ec)
{
   if (ec->visibility.changed)
     {
        if (ec->visibility.obscured == E_VISIBILITY_UNOBSCURED)
          {
             _e_processmgr_client_thaw(ec);
          }
     }
}

static Eina_Bool
_e_processmgr_client_freeze_condition_check(E_Client *ec)
{
   E_WindowInfo *winfo  = NULL;
   E_ProcessInfo *pinfo  = NULL;
   E_Client *temp_ec = NULL;
   Eina_Bool freeze = EINA_TRUE;
   Eina_List *l;

   if (!ec) return EINA_FALSE;

   winfo = _e_processmgr_client_info_find(_pm, ec);
   if (!winfo) return EINA_FALSE;

   if (winfo->pid <= 0) return EINA_FALSE;

   pinfo = _e_processmgr_processinfo_find(_pm, winfo->pid);
   if (!pinfo) return EINA_FALSE;

   if (pinfo->state == PROCESS_STATE_BACKGROUND) return EINA_FALSE;
   if (!pinfo->wins) return EINA_FALSE;

   EINA_LIST_FOREACH(pinfo->wins, l, temp_ec)
     {
        if (!temp_ec->iconic)
          {
             freeze = EINA_FALSE;
             break;
          }
     }

   return freeze;
}

static Eina_Bool
_e_processmgr_client_freeze(E_Client *ec)
{
   E_WindowInfo   *winfo  = NULL;
   E_ProcessInfo  *pinfo  = NULL;

   if (!ec) return EINA_FALSE;

   winfo = _e_processmgr_client_info_find(_pm, ec);
   if (!winfo) return EINA_FALSE;

   if (winfo->pid <= 0) return EINA_FALSE;

   pinfo = _e_processmgr_processinfo_find(_pm, winfo->pid);
   if (!pinfo) return EINA_FALSE;

   if (pinfo->state != PROCESS_STATE_BACKGROUND)
     {
        ELOGF("PROCESSMGR STATE", "PROCESS_BACKGROUND. PID:%d", NULL, NULL, winfo->pid);
        pinfo->state = PROCESS_STATE_BACKGROUND;
        _e_processmgr_send_pid_action(winfo->pid, PROCESS_BACKGROUND);
     }

   return EINA_TRUE;
}

static Eina_Bool
_e_processmgr_client_thaw(E_Client *ec)
{
   E_WindowInfo   *winfo  = NULL;
   E_ProcessInfo  *pinfo  = NULL;

   if (!ec) return EINA_FALSE;

   winfo = _e_processmgr_client_info_find(_pm, ec);
   if (!winfo) return EINA_FALSE;

   if (winfo->pid <= 0) return EINA_FALSE;

   pinfo = _e_processmgr_processinfo_find(_pm, winfo->pid);
   if (!pinfo) return EINA_FALSE;

   if (pinfo->state != PROCESS_STATE_FOREGROUND)
     {
        ELOGF("PROCESSMGR STATE", "PROCESS_FOREGROUND. PID:%d", NULL, NULL, winfo->pid);
        pinfo->state = PROCESS_STATE_FOREGROUND;
        _e_processmgr_send_pid_action(winfo->pid, PROCESS_FOREGROUND);
     }

   return EINA_TRUE;
}

static void
_e_processmgr_send_pid_action(int pid, E_ProcessMgr_Action act)
{
   Eldbus_Connection *conn;
   Eldbus_Message *msg;

   int param_pid;
   int param_act;

   conn = eldbus_connection_get(ELDBUS_CONNECTION_TYPE_SYSTEM);
   if (!conn) return;

   // set up msg for resourced
   msg = eldbus_message_signal_new("/Org/Tizen/ResourceD/Process",
                                   "org.tizen.resourced.process",
                                   "ProcStatus");
   if (!msg) return;

   // append the action to do and the pid to do it to
   param_pid = (int)pid;
   param_act = (int)act;

   if (!eldbus_message_arguments_append(msg, "ii", param_pid, param_act))
     {
        eldbus_message_unref(msg);
        return;
     }

   // send the message
   if (!eldbus_connection_send(conn, msg, NULL, NULL, -1))
     return;

   eldbus_message_unref(msg);
}


EAPI Eina_Bool
e_mod_processmgr_init(void)
{
   E_ProcessMgr *pm;
   E_Client_Hook *hook;

   if (!eldbus_init())
     return EINA_FALSE;

   pm = _e_processmgr_new();
   if (!pm)
     {
        eldbus_shutdown();
        return EINA_FALSE;
     }

   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_ADD, _e_processmgr_cb_client_add, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_REMOVE, _e_processmgr_cb_client_remove, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_ICONIFY, _e_processmgr_cb_client_iconify, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_UNICONIFY, _e_processmgr_cb_client_uniconify, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_VISIBILITY_CHANGE, _e_processmgr_cb_client_visibility_change, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_FOCUS_IN, _e_processmgr_cb_client_focus_in, NULL);

   hook = e_client_hook_add(E_CLIENT_HOOK_EVAL_VISIBILITY, _pol_cb_hook_client_visibility, NULL);
   if (hook) hooks_ec = eina_list_append(hooks_ec, hook);

   _pm = pm;

   return EINA_TRUE;

}

EAPI void
e_mod_processmgr_shutdown(void)
{
   E_Client_Hook *hook;

   if (!_pm) return;

   eldbus_shutdown();

   E_FREE_LIST(handlers, ecore_event_handler_del);
   EINA_LIST_FREE(hooks_ec, hook)
     e_client_hook_del(hook);

   _e_processmgr_del(_pm);
   _pm = NULL;
}

