#ifndef E_MOD_PROCESSMGR_H
#define E_MOD_PROCESSMGR_H

typedef struct _E_Processmgr_Info E_Processmgr_Info;

struct _E_Processmgr_Info
{
   E_Comp *comp;

   Eina_Hash *clients;
};

EAPI Eina_Bool e_mod_processmgr_init(void);
EAPI void e_mod_processmgr_shutdown(void);

#endif

