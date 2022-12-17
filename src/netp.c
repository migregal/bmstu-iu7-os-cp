#include <linux/module.h>

#include "netp.h"

// params ----------------------------------------------------------------------

static char *net_modules[16] = {"iwlwifi"};
static int net_modules_n = 1;
module_param_array(net_modules, charp, &net_modules_n, S_IRUGO);
MODULE_PARM_DESC(net_modules, "List of modules to manipulate with");

// params ^---------------------------------------------------------------------

static bool is_network_down = false;

bool
is_network_disabled(void)
{
  return is_network_down;
}

static char *envp[] = {"HOME=/", "TERM=linux", "PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL};

void
disable_network(void)
{
  int i = 0;
  for (; i < net_modules_n; i++)
  {
    char *argv[] = {"/sbin/modprobe", "-r", net_modules[i], NULL};
    if (call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC > 0))
    {
      pr_warn("netpmod: unable to kill network\n");
    }
    else
    {
      pr_info("netpmod: network is killed\n");
      is_network_down = true;
    }
  }
}

void
enable_network(void)
{
  int i = 0;
  for (; i < net_modules_n; i++)
  {
    char *argv[] = {"/sbin/modprobe", net_modules[i], NULL};
    if (call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC > 0))
    {
      pr_warn("netpmod: unable to bring network back\n");
    }
    else
    {
      pr_info("netpmod: network is available now\n");
      is_network_down = false;
    }
  }
}
