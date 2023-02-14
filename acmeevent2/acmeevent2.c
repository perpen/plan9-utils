#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <9pclient.h>
#include <auth.h>
#include <thread.h>
#include <stdio.h>
#include <bio.h>


char *winid;
Channel *chan;
CFsys *fs;

int
getn(Biobuf *b)
{
	int c, n;

	n = 0;
	while((c = Bgetc(b)) != -1 && '0'<=c && c<='9')
		n = n*10+c-'0';
	if(c != ' ')
		sysfatal("bad number syntax");
	return n;
}

char*
getrune(Biobuf *b, char *p)
{
	int c;
	char *q;

	c = Bgetc(b);
	if(c == -1)
		sysfatal("eof");
	q = p;
	*q++ = c;
	if(c >= Runeself){
		while(!fullrune(p, q-p)){
			c = Bgetc(b);
			if(c == -1)
				sysfatal("eof");
			*q++ = c;
		}
	}
	return q;
}

void
getevent(Biobuf *b, int *c1, int *c2, int *q0, int *q1, int *flag, int *nr, char *buf)
{
	int i;
	char *p;

	*c1 = Bgetc(b);
	if(*c1 == -1) // was window deleted
		threadexitsall(0);
	*c2 = Bgetc(b);
	*q0 = getn(b);
	*q1 = getn(b);
	*flag = getn(b);
	*nr = getn(b);
	if(*nr >= 256)
		sysfatal("event string too long");
	p = buf;
	for(i=0; i<*nr; i++)
		p = getrune(b, p);
	*p = 0;
	if(Bgetc(b) != '\n')
		sysfatal("expected newline");
}

void
eventer()
{
	int c1, c2, q0, q1, eq0, eq1, flag, nr, x, fd;
	Biobuf b;
	char buf[2000], buf2[2000], buf3[2000], path[1000], msg[8000];

	assert(snprint(path, sizeof(path), "%s/event", winid) < sizeof(path));
    fd = fsopenfd(fs, path, OREAD);
    if(fd < 0){
        sysfatal("acmedot: cannot open %s: %r", buf);
    }
	assert(Binit(&b, fd, OREAD) >= 0);
	for(;;){
		getevent(&b, &c1, &c2, &q0, &q1, &flag, &nr, buf);
		eq0 = q0;
		eq1 = q1;
		buf2[0] = 0;
		buf3[0] = 0;
		if(flag & 2){
			/* null string with non-null expansion */
			getevent(&b, &x, &x, &eq0, &eq1, &x, &nr, buf);
		}
		if(flag & 8){
			/* chorded argument */
			getevent(&b, &x, &x, &x, &x, &x, &x, buf2);
			getevent(&b, &x, &x, &x, &x, &x, &x, buf3);
		}
		snprint(msg, sizeof(msg),
			"handleevent %c %c %d %d %d %d %d %d %q %q %q",
			c1, c2, q0, q1, eq0, eq1, flag, nr, buf, buf2, buf3);
		if(c1 != 'F') // F: our use of the acme files
			sendp(chan, msg);
	}
}

void
logger()
{
	Biobuf b;
	char *line;
	char msg[1000], id[10], op[20], name[1000];
	int fd;

    fd = fsopenfd(fs, "log", OREAD);
    if(fd < 0){
        sysfatal("acmedot: cannot open log: %r");
    }
	assert(Binit(&b, fd, OREAD) >= 0);
	for(;;){
		line = Brdline(&b, '\n');
		assert(line != 0);
		line[Blinelen(&b)] = '\0';
		name[0] = '\0';
		sscanf(line, "%s %s %s", id, op, name);
		snprint(msg, sizeof(msg), "handlelog %s %s %q\n", id, op, name);
		sendp(chan, msg);
	}
}

static void
usage(void)
{
	fprint(2, "usage: %s [-l]\n", argv0);
	threadexitsall("usage");
}

void
threadmain(int argc, char **argv)
{
	char *msg;
	int dolog = 0;

	doquote = needsrcquote;
	quotefmtinstall();
	winid = getenv("winid");

	ARGBEGIN{
	case 'l':
		dolog = 1;
		break;
	default:
		usage();
	}ARGEND

	fs = nsmount("acme", nil);
	if(fs == nil){
		sysfatal("acmedot: %r");
	}

	chan = chancreate(sizeof(char*), 0);
	if(dolog)
		proccreate(logger, nil, 16*1024);
	proccreate(eventer, nil, 16*1024);
	for(;;){
		msg = recvp(chan);
		// fprintf(stderr, "printing: %s\n", msg);
		print("%s\n", msg);
	}
}
