#include <u.h>
#include <libc.h>
#include <stdio.h>

void
usage(void)
{
	fprint(2, "usage: %s [-n] [string ...]\n", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	int i, newline = 1;
	
	ARGBEGIN{
	case 'n':
		newline = 0;
		break;
	default:
		usage();
	}ARGEND;
	if(argc == 0)
		usage();
	doquote = needsrcquote;
	for(i = 0; i < argc; i++){
		printf("%s%s", quotestrdup(argv[i]), i == argc -1 ? "" : " ");
	}
	if(newline)
		printf("\n");
	exits(nil);
}
