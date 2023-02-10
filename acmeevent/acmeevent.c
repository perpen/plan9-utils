/* From plan9port, with changes:
	- new option "-l" to watch the acme log
	- the commands emitted are handleevent and handlelog
	- ignores F events
*/
#include <u.h>
#include <libc.h>
#include <thread.h>
#include <stdio.h>
#include <bio.h>

char *winid;
Channel *chan;

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
	if(*c1 == -1)
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
	int c1, c2, q0, q1, eq0, eq1, flag, nr, x, eventpathlen;
	Biobuf *b;
	char buf[2000], buf2[2000], buf3[2000], eventpath[1000], msg[8000];

	eventpathlen = snprintf(eventpath, sizeof(eventpath), "/mnt/acme/%s/event", winid);
	assert(eventpathlen < sizeof(eventpath));
	b = Bopen(eventpath, OREAD);
	assert(b != nil);

	for(;;){
		getevent(b, &c1, &c2, &q0, &q1, &flag, &nr, buf);
		eq0 = q0;
		eq1 = q1;
		buf2[0] = 0;
		buf3[0] = 0;
		if(flag & 2){
			/* null string with non-null expansion */
			getevent(b, &x, &x, &eq0, &eq1, &x, &nr, buf);
		}
		if(flag & 8){
			/* chorded argument */
			getevent(b, &x, &x, &x, &x, &x, &x, buf2);
			getevent(b, &x, &x, &x, &x, &x, &x, buf3);
		}
		snprint(msg, sizeof(msg),
			"handleevent %c %c %d %d %d %d %d %d %q %q %q",
			c1, c2, q0, q1, eq0, eq1, flag, nr, buf, buf2, buf3);
		if(c1 != 'F') // probably our use of the acme files
			sendp(chan, msg);
	}
}

void
logger()
{
	Biobuf *b;
	char *line;
	char msg[1000], id[10], op[20], name[1000];

	b = Bopen("/mnt/acme/log", OREAD);
	assert(b != nil);
	for(;;){
		line = Brdline(b, '\n');
		assert(line != 0);
		line[Blinelen(b)] = '\0';
		name[0] = '\0';
		sscanf(line, "%s %s %s", id, op, name);
		sprint(msg, "handlelog %s %s %q\n", id, op, name);
		sendp(chan, msg);
	}
}

static void
usage(void)
{
	fprint(2, "usage: %s [-l]\n", argv0);
	exits("usage");
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

	chan = chancreate(sizeof(char*), 0);
	if(dolog)
		proccreate(logger, nil, 16*1024);
	proccreate(eventer, nil, 16*1024);
	for(;;){
		msg = recvp(chan);
		print("%s\n", msg);
	}
}
