/*
 * vorbis_comment.c
 *
 * A simple example on how to modify Vorbis Comments with libogg and libvorbis.
 * Compile with:
 *   cc -I/include/path vorbis_comment.c -L/lib/path -logg -lvorbis -lvorbisfile
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "ogg/ogg.h"
#include "vorbis/codec.h"
#define	OV_EXCLUDE_STATIC_CALLBACKS 1
#include "vorbis/vorbisfile.h"
#undef	OV_EXCLUDE_STATIC_CALLBACKS


/*
 * write the page p into the given file pointer fp.
 */
void
write_page(ogg_page *p, FILE *fp)
{

	assert(fwrite(p->header, 1, p->header_len, fp) == p->header_len);
	assert(fwrite(p->body,   1, p->body_len,   fp) == p->body_len);
}


/*
 * copy a ogg/vorbis file from path_in to path_out, using the given Vorbis Comments
 * vc_out for the new file.
 */
void
save_it(const char *path_in, struct vorbis_comment *vc_out, const char *path_out)
{
	FILE             *fp_in;  /* input file pointer */
	FILE             *fp_out; /* output file pointer */
	ogg_sync_state    oy_in;  /* sync and verify incoming physical bitstream */
	ogg_stream_state  os_in;  /* take physical pages, weld into a logical
	                             stream of packets */
	ogg_stream_state  os_out; /* take physical pages, weld into a logical
	                             stream of packets */
	ogg_page          og_in;  /* one Ogg bitstream page. Vorbis packets are inside */
	ogg_page          og_out; /* one Ogg bitstream page. Vorbis packets are inside */
	ogg_packet        op_in;  /* one raw packet of data for decode */
	ogg_packet        my_vc_packet; /* our custom packet containing vc_out */
	vorbis_info       vi_in;  /* struct that stores all the static vorbis
	                             bitstream settings */
	vorbis_comment    vc_in;  /* struct that stores all the bitstream user
	                             comment */
	unsigned long     nstream_in; /* stream counter */
	unsigned long     npage_in;   /* page counter */
	unsigned long     npacket_in; /* packet counter */
	unsigned long     bs;     /* blocksize of the current packet */
	unsigned long     lastbs; /* blocksize of the last packet */
	ogg_int64_t       granulepos; /* granulepos of the current page */
	enum { NEEDFLUSH, NEEDOUT, NOTHING } state;

	/* create the packet holding our vorbis_comment */
	assert(vorbis_commentheader_out(vc_out, &my_vc_packet) == 0);

	/* open files & init stuff */
	(void)ogg_sync_init(&oy_in); /* always return 0 */
	assert(fp_in = fopen(path_in, "r"));
	assert(fp_out = (path_out == NULL ? stdout : fopen(path_out, "w")));
	lastbs = granulepos = 0;

	nstream_in = 0;
bos_label: /* beginning of a stream */
	nstream_in += 1;
	npage_in = npacket_in = 0;
	vorbis_info_init(&vi_in);
	vorbis_comment_init(&vc_in);

	/*
	 * main loop: read the input file (case 0 and -1) in order to sync pages
	 * out. Once a page is available, we will go through each packets.
	 */
	for (;;) {
		switch (ogg_sync_pageout(&oy_in, &og_in)) {
		case 0:  /* more data needed or an internal error occurred. */
		case -1: /* stream has not yet captured sync (bytes were skipped). */
			if (feof(fp_in)) {
				/*
				 * There is no more data to read and we could
				 * not get a page so we're done here.
				 */
				goto eos_label;
			} else {
				/*
				 * read more data and try again to get a page.
				 */
				char *buf;
				size_t s;
				/* get a buffer */
				assert(buf = ogg_sync_buffer(&oy_in, BUFSIZ));
				/* read a part of the file */
				assert((s = fread(buf, sizeof(char), BUFSIZ, fp_in)) != -1);
				/* tell ogg how much was read */
				assert(ogg_sync_wrote(&oy_in, s) == 0);
				/* try again to sync a page out */
				continue;
			}
			/* NOTREACHED */
		}
		/* here ogg_sync_pageout() returned 1 and a page was sync'ed. */
		if (++npage_in == 1) {
			/* init both input and output streams with the serialno
			   of the first page read. */
			assert(ogg_stream_init(&os_in, ogg_page_serialno(&og_in)) == 0);
			assert(ogg_stream_init(&os_out, ogg_page_serialno(&og_in)) == 0);
		}

		/* put the page in input stream, and then loop through each its
		   packet(s) */
		assert(ogg_stream_pagein(&os_in, &og_in) == 0);
		while (ogg_stream_packetout(&os_in, &op_in) == 1) {
			ogg_packet *target;
			/*
			 * This is where we really do what we mean to do: the
			 * second packet is the commentheader packet, we replace
			 * it with my_vc_packet if we're on the first stream.
			 */
			if (++npacket_in == 2 && nstream_in == 1)
				target = &my_vc_packet;
			else
				target = &op_in;

			if (npacket_in <= 3) {
				/*
				 * The first three packets are header packets.
				 * We use them to get the vorbis_info which
				 * will be used later. vc_in is unused.
				 */
				vorbis_synthesis_headerin(&vi_in, &vc_in, &op_in);
				/*
				 * We force a flush after the header packets, so
				 * the data stream start on a fresh page.
				 */
				state = NEEDFLUSH;
			} else {
				/*
				 * granulepos computation.
				 *
				 * The granulepos is stored into the *pages* and
				 * is used by the codec to seek through the
				 * bitstream.  Its value is codec dependent (in
				 * the Vorbis case it is the number of samples
				 * elapsed).
				 *
				 * The vorbis_packet_blocksize() actually
				 * compute the number of sample that would be
				 * stored by the packet (without decoding it).
				 * This is the same formula as in vcedit example
				 * from vorbis-tools.
				 *
				 * We use here the vorbis_info previously filled
				 * when reading header packets.
				 *
				 * XXX: check if this is not a vorbis stream ?
				 */
				bs = vorbis_packet_blocksize(&vi_in, &op_in);
				granulepos += (lastbs == 0 ? 0 : (bs + lastbs) / 4);
				lastbs = bs;

				/* write page(s) if needed */
				if (state == NEEDFLUSH) {
					while (ogg_stream_flush(&os_out, &og_out))
						write_page(&og_out, fp_out);
				} else if (state == NEEDOUT) {
					while (ogg_stream_pageout(&os_out, &og_out))
						write_page(&og_out, fp_out);
				}

				/*
				 * Decide wether we need to write a page based
				 * on our granulepos computation. The -1 case is
				 * very common because only the last packet of a
				 * page has its granulepos set by the ogg layer
				 * (which only store a granulepos per page), so
				 * all the other have a value of -1 (we need to
				 * set the granulepos for each packet though).
				 *
				 * The other cases logic are borrowed from
				 * vcedit and I fail to see how granulepos could
				 * mismatch because we don't alter the data
				 * packet.
				 */
				state = NOTHING;
				if (op_in.granulepos == -1) {
					op_in.granulepos = granulepos;
				} else if (granulepos <= op_in.granulepos) {
					state = NEEDOUT;
				} else /* if granulepos > op_in.granulepos */ {
					state = NEEDFLUSH;
					granulepos = op_in.granulepos;
				}
			}
			/* insert the target packet into the output stream */
			assert(ogg_stream_packetin(&os_out, target) == 0);
		}
		if (ogg_page_eos(&og_in)) {
			/* og_in was the last page of the stream */
			goto eos_label;
		}
	}

eos_label: /* end of a stream */

	/* ensure we did read at least all Vorbis headers */
	assert(npacket_in >= 3);

	/* forces remaining packets into page(s) and write all of them in the
	   output file */
	os_out.e_o_s = 1;
	while (ogg_stream_flush(&os_out, &og_out))
		write_page(&og_out, fp_out);

	/* stream cleanup */
	/* ogg_page and ogg_packet structs always point to storage in libvorbis.
	   They're never freed or manipulated directly */
	vorbis_comment_clear(&vc_in);
	vorbis_info_clear(&vi_in);
	ogg_stream_clear(&os_in);
	ogg_stream_clear(&os_out);

	if (!feof(fp_in))
		goto bos_label;

	/* We reached feof, cleanup */
	ogg_packet_clear(&my_vc_packet);
	ogg_sync_clear(&oy_in);
	if (fp_out != stdout)
		assert(fclose(fp_out) == 0);
	(void)fclose(fp_in);
}


int
main(int argc, char *argv[])
{
	const char *path_in, *path_out;
	struct OggVorbis_File vf;
	struct vorbis_comment *vc;
	int i;

	if (argc == 2) {
		path_in  = argv[1];
		path_out = NULL;
	} else if (argc == 3) {
		path_in  = argv[1];
		path_out = argv[2];
	} else {
		(void)fprintf(stderr, "usage: %s file [output]\n", argv[0]);
		return (1);
	}

	/* try to read path_in as an ogg/vorbis file */
	if ((i = ov_fopen(path_in, &vf) != 0)) {
		(void)fprintf(stderr, "%s: can't open as ogg/vorbis file.\n", path_in);
		return (1);
	}
	/* get the vorbis comments */
	assert(vc = ov_comment(&vf, -1));

	/* change something */
	vorbis_comment_add(vc, "test=42");

	/* display vorbis comments to stderr (in case stdin is used as output) */
	for (i = 0; i < vc->comments; i++)
		(void)fprintf(stderr, "%s\n", vc->user_comments[i]);

	/* now save the modified comments (and copy audio data) into path_out */
	save_it(path_in, vc, path_out);

	/* cleanup */
	ov_clear(&vf);
	return (0);
}
