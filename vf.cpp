/*
 * Copyright (c) 1994 Paul Vojta.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define DEBUG 0

#include <kdebug.h>
#include <stdio.h>

#include "dviwin.h"
#include "oconfig.h"
#include "dvi.h"

extern char *xmalloc (unsigned, _Xconst char *);
extern font *define_font(FILE *file, unsigned int cmnd, font *vfparent, QIntDict<struct font> *TeXNumberTable);

/***
 ***	VF font reading routines.
 ***	Public routine is read_index---because virtual characters are presumed
 ***	to be short, we read the whole virtual font in at once, instead of
 ***	faulting in characters as needed.
 ***/

#define	LONG_CHAR	242

/*
 *	These are parameters which determine whether macros are combined for
 *	storage allocation purposes.  Small macros ( <= VF_PARM_1 bytes) are
 *	combined into chunks of size VF_PARM_2.
 */

#ifndef	VF_PARM_1
#define	VF_PARM_1	20
#endif
#ifndef	VF_PARM_2
#define	VF_PARM_2	256
#endif

/*
 *	The main routine
 */

void read_VF_index(font *fontp, wide_bool hushcs)
{
  FILE *VF_file = fontp->file;
  unsigned char	cmnd;
  unsigned char	*avail, *availend;	/* available space for macros */
  long          checksum;

  fontp->read_char   = NULL;
  fontp->flags      |= FONT_VIRTUAL;
  fontp->set_char_p  = set_vf_char;
  //@@@ if (_debug & DBG_PK)
  kDebugInfo("Reading VF pixel file %s", fontp->filename);

  // Read preamble.
  Fseek(VF_file, (long) one(VF_file), 1);	/* skip comment */
  checksum = four(VF_file);
  if (!hushcs && checksum && fontp->checksum && checksum != fontp->checksum)
    kDebugError("Checksum mismatch (dvi = %lu, vf = %lu) in font file %s", 
		fontp->checksum, checksum, fontp->filename);
  (void) four(VF_file);		/* skip design size */

  // Read the fonts.
  /*@@@
    fontp->vf_table = (struct font **)xmalloc(VFTABLELEN * sizeof(struct font *), "table of VF TeXnumbers");
    bzero((char *) fontp->vf_table, VFTABLELEN * sizeof(struct font *));
    fontp->vf_chain = NULL;
  */
  fontp->first_font = NULL;

  while ((cmnd = one(VF_file)) >= FNTDEF1 && cmnd <= FNTDEF4) {
    struct font *newfontp = define_font(VF_file, cmnd, fontp, &(fontp->vf_table));
    if (fontp->first_font == NULL)
      fontp->first_font = newfontp;
  }

  // Prepare macro array.
  fontp->macro = (struct macro *) xmalloc(256 * sizeof(struct macro),"macro array");
  bzero((char *) fontp->macro, 256 * sizeof(struct macro));

  // Read macros.
  avail = availend = NULL;
  for (; cmnd <= LONG_CHAR; cmnd = one(VF_file)) {
    register struct macro *m;
    int len;
    unsigned long cc;
    long width;

    if (cmnd == LONG_CHAR) {	/* long form packet */
      len = four(VF_file);
      cc = four(VF_file);
      width = four(VF_file);
      if (cc >= 256) {
	kDebugError(TRUE,4300,"Virtual character %lu in font %s ignored.", cc, fontp->fontname);
	Fseek(VF_file, (long) len, 1);
	continue;
      }
    } else {	/* short form packet */
      len = cmnd;
      cc = one(VF_file);
      width = num(VF_file, 3);
    }
    m = &fontp->macro[cc];
    m->dvi_adv = width * fontp->dimconv;
    if (len > 0) {
      if (len <= availend - avail) {
	m->pos = avail;
	avail += len;
      } else {
	m->free_me = True;
	if (len <= VF_PARM_1) {
	  m->pos = avail = (unsigned char *) xmalloc(VF_PARM_2, "macro array");
	  availend = avail + VF_PARM_2;
	  avail += len;
	} else 
	  m->pos = (unsigned char *) xmalloc((unsigned) len, "macro array");
      }
      Fread((char *) m->pos, 1, len, VF_file);
      m->end = m->pos + len;
    }
    kDebugInfo(DEBUG, 4300, "Read VF macro for character %lu; dy = %ld, length = %d",cc, m->dvi_adv, len);
  }
  if (cmnd != POST)
    oops("Wrong command byte found in VF macro list:  %d", cmnd);
	
  Fclose (VF_file);
  n_files_left++;
  fontp->file = NULL;
}
