#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

int execs(char *progname, int argc, ...) {
	char *cmdline, *tmp, *arg;
	size_t cmdlen, len;
	va_list ap;
	int i, result;

	if (argc == 0) {
		cmdline = progname;
		goto run_prog;
	}

	cmdlen = strlen(progname) + 1;
	va_start(ap, argc);
	for (i = 0; i < argc; ++i)
		cmdlen += strlen(va_arg(ap, char *)) + 1;
	va_end(ap);

	if (!(cmdline = malloc(cmdlen)))
		return -1;
	tmp = cmdline;

	len = strlen(progname);
	memcpy(tmp, progname, len);
	tmp[len] = ' ';
	tmp += len + 1;

	va_start(ap, argc);
	for (i = 0; i < argc; ++i) {
		arg = va_arg(ap, char *);
		len = strlen(arg);
		memcpy(tmp, arg, len);
		tmp[len] = ' ';
		tmp += len + 1;
	}
	va_end(ap);
	cmdline[cmdlen-1] = '\0';

run_prog:
	result = system(cmdline);
	if (!WIFEXITED(result)) {
		return -1;
	} else {
		return WEXITSTATUS(result);
	}
}
