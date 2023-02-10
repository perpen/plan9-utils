#include <u.h>
#include <libc.h>
#include <stdio.h>

void
main(int argc, char **argv)
{
	doquote = needsrcquote;
	quotefmtinstall();
	printf("test: %q\n", "a b");
}
