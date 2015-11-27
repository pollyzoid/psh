#include "shell.h"

#include <stdio.h>

shell_state *psh = NULL;

int main() {
	bool ok = (psh = shell_init());

	if (!ok) {
		return -1;
	}

	while (ok) {
		ok = shell_cmdloop(psh);
	}

	shell_destroy(psh);
	return 0;
}
