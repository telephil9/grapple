#include <u.h>
#include <libc.h>
#include <ctype.h>
#include <bio.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>
#include <plumb.h>
#include <regexp.h>

typedef struct Line Line;

struct Line
{
	char *text;
	char ptext[255];
};

enum
{
	Emouse,
	Eresize,
	Ekeyboard,
};

enum
{
	Maxlines = 4096,
	Padding = 4,
	Scrollwidth = 12,
};

Mousectl *mctl;
Keyboardctl *kctl;
Image *selbg;
Image *mfg;
Rectangle ir;
Rectangle sr;
Rectangle lr;
int lh;
int lcount;
int loff;
int lsel;
int scrollsize;
int qmode;
int plumbfd;
Line lines[Maxlines];
int nlines;
char pwd[255] = {0};
Reprog *re;

void
loadlines(void)
{
	Biobuf *bp;
	char *s;
	Resub sub;
	char buf[255];
	int n;

	nlines = 0;
	bp = Bfdopen(0, OREAD);
	for(;;){
		s = Brdstr(bp, '\n', 1);
		if(s == nil)
			break;
		lines[nlines].text = s;
		memset(&sub, 0, sizeof(sub));
		if(regexec(re, s, &sub, 1) != 0)
			snprint(lines[nlines].ptext, sizeof(lines[nlines].ptext), "%.*s", (int)(sub.ep - sub.sp), sub.sp);
		else
			lines[nlines].ptext[0] = 0;
		nlines += 1;
		if(nlines >= Maxlines)
			break;
	}
	Bterm(bp);
}

void
activate(void)
{
	Line l;

	l = lines[lsel + loff];
	if(l.ptext[0] == 0)
		return;
	plumbsendtext(plumbfd, argv0, nil, pwd, l.ptext);
	if(qmode)
		threadexitsall(nil);
}

int
lineat(Point p)
{
	if(ptinrect(p, lr) == 0)
		return -1;
	return (p.y - lr.min.y) / lh;	
}

Rectangle
linerect(int i)
{
	Rectangle r;

	r.min.x = 0;
	r.min.y = i * (font->height + Padding);
	r.max.x = Dx(lr) - 2*Padding;
	r.max.y = (i + 1) * (font->height + Padding);
	r = rectaddpt(r, lr.min);
	return r;
}

void
drawline(int i, int sel)
{
	Point p;
	char *t;

	if(loff + i >= nlines)
		return;
	draw(screen, linerect(i), sel ? selbg : display->white, nil, ZP);
	p = addpt(lr.min, Pt(0, i * lh));
	t = lines[loff + i].text;
	while(*t){
		if(*t == '\t')
			p = string(screen, p, display->black, ZP, font, "    ");
		else
			p = stringn(screen, p, display->black, ZP, font, t, 1);
		t++;
	}
}

void
redraw(void)
{
	Rectangle scrposr;
	int i, h, y, ye;

	draw(screen, screen->r, display->white, nil, ZP);
	draw(screen, sr, mfg, nil, ZP);
	border(screen, sr, 0, display->black, ZP);
	if(nlines > 0){
		h = ((double)lcount / nlines) * Dy(sr);
		y = ((double)loff / nlines) * Dy(sr);
		ye = sr.min.y + y + h - 1;
		if(ye >= sr.max.y)
			ye = sr.max.y - 1;
		scrposr = Rect(sr.min.x + 1, sr.min.y + y + 1, sr.max.x - 1, ye);
	}else
		scrposr = insetrect(sr, -1);
	draw(screen, scrposr, display->white, nil, ZP);
	for(i = 0; i < lcount; i++)
		drawline(i, i == lsel);
	flushimage(display, 1);
}

int
scroll(int lines, int setsel)
{
	if(nlines <= lcount)
		return 0;
	if(lines < 0 && loff == 0)
		return 0;
	if(lines > 0 && loff + lcount >= nlines){
		return 0;
	}
	loff += lines;
	if(loff < 0)
		loff = 0;
	if(loff + nlines%lcount >= nlines)
		loff = nlines - nlines%lcount;
	if(setsel){
		if(lines > 0)
			lsel = 0;
		else
			lsel = lcount - 1;
	}
	redraw();
	return 1;
}

void
changesel(int from, int to)
{
	drawline(from, 0);
	drawline(to, 1);
	flushimage(display, 1);
}

void
eresize(void)
{
	Rectangle r;

	r = screen->r;
	sr = Rect(r.min.x + Padding, r.min.y + Padding, r.min.x + Padding + Scrollwidth, r.max.y - Padding - 1);
	lr = Rect(sr.max.x + Padding, r.min.y + Padding, r.max.x, r.max.y - Padding);
	lh = font->height + Padding;
	lcount = Dy(lr) / lh;
	scrollsize = mousescrollsize(lcount);
	redraw();
}

void
emouse(Mouse *m)
{
	int n;

	if(ptinrect(m->xy, lr)){
		if(m->buttons == 1 || m->buttons == 4){
			n = lineat(m->xy);
			if(n != -1 && (loff + n) < nlines){
				changesel(lsel, n);
				lsel = n;
			}
			if(m->buttons == 4)
				activate();
		}else if(m->buttons == 8){
			scroll(-scrollsize, 1);
		}else if(m->buttons == 16){
			scroll(scrollsize, 1);
		}
	}else if(ptinrect(m->xy, sr)){
		if(m->buttons == 1){
			n = (m->xy.y - sr.min.y) / lh;
			scroll(-n, 1);
		}else if(m->buttons == 2){
			n = (m->xy.y - sr.min.y) * nlines / Dy(sr);
			loff = n;
			redraw();
		}else if(m->buttons == 4){
			n = (m->xy.y - sr.min.y) / lh;
			scroll(n, 1);
		}
	}
}

void
ekeyboard(Rune k)
{
	int osel;

	switch(k){
	case 'q':
	case Kesc:
	case Kdel:
		threadexitsall(nil);
		break;
	case Kup:
		if(lsel == 0 && loff > 0)
			scroll(-lcount, 1);
		else if(lsel > 0)
			changesel(lsel, --lsel);
		break;
	case Kdown:
		if(lsel < (nlines - 1)){
			if(lsel == lcount - 1)
				scroll(lcount, 1);
			else if(loff + lsel < nlines - 1)
				changesel(lsel, ++lsel);
		}
		break;
	case Kpgup:
		scroll(-lcount, 1);
		break;
	case Kpgdown:
		scroll(lcount, 1);
		break;
	case Khome:
		osel = lsel;
		lsel = 0;
		if(scroll(-nlines, 0) == 0)
			changesel(osel, lsel);
		break;
	case Kend:
		osel = lsel;
		lsel = nlines%lcount - 1;
		if(scroll(nlines, 0) == 0){
			changesel(osel, lsel);
		}
		break;
	case '\n':
		if(lsel >= 0)
			activate();
		break;
	}
}

void
usage(void)
{
	fprint(2, "usage: %s [-q]\n", argv0);
	exits("usage");
}

void
threadmain(int argc, char **argv)
{
	Mouse m;
	Rune k;
	Alt a[] = {
		{ nil, &m, CHANRCV },
		{ nil, nil, CHANRCV },
		{ nil, &k, CHANRCV },
		{ nil, nil, CHANEND },
	};

	qmode = 0;
	ARGBEGIN{
	case 'q':
		qmode = 1;
		break;
	default:
		usage();
	}ARGEND;
	plumbfd = plumbopen("send", OWRITE|OCEXEC);
	if(plumbfd < 0)
		sysfatal("plumbopen: %r");
	re = regcomp("([.a-zA-Z0-9_/+\\-]+:[0-9]+)");
	getwd(pwd, sizeof pwd);
	loadlines();
	if(initdraw(nil, nil, "grapple") < 0)
		sysfatal("initdraw: %r");
	display->locking = 0;
	if((mctl = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");
	if((kctl = initkeyboard(nil)) == nil)
		sysfatal("initkeyboard: %r");
	a[Emouse].c = mctl->c;
	a[Eresize].c = mctl->resizec;
	a[Ekeyboard].c = kctl->c;
	selbg = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xEFEFEFFF);
	mfg = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0x999999FF);
	loff = 0;
	lsel = 0;
	eresize();
	for(;;){
		switch(alt(a)){
		case Emouse:
			emouse(&m);
			break;
		case Eresize:
			if(getwindow(display, Refnone) < 0)
				sysfatal("getwindow: %r");
			eresize();
			break;
		case Ekeyboard:
			ekeyboard(k);
			break;
		}
	}
}
