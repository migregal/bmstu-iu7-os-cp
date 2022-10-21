#define UMH_NO_WAIT     0 // don't wait at all
#define UMH_WAIT_EXEC   1 // wait for the exec, but not the process
#define UMH_WAIT_PROC   2 // wait for the process to complete
#define UMH_KILLABLE    4 // wait for EXEC/PROC killable

struct subprocess_info {
	struct work_struct work;
	struct completion *complete;
	const char *path;
	char **argv;
	char **envp;
	int wait;
	int retval;
	int (*init)(struct subprocess_info *info, struct cred *new);
	void (*cleanup)(struct subprocess_info *info);
	void *data;
} __randomize_layout;

extern int call_usermodehelper(const char *path, char **argv, char **envp, int wait);
