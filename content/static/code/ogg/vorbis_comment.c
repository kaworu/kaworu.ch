/*
 * vorbis_comment.c
 *
 * A simple example on how to modify Vorbis Comments with libogg and libvorbis.
 * Compile with:
 *   cc -I/include/path vorbis_comment.c -L/lib/path -logg -lvorbis -lvorbisfile
 */
#include <stdio.h>
#include <stdlib.h>

#include "ogg/ogg.h"
#include "vorbis/codec.h"
#define	OV_EXCLUDE_STATIC_CALLBACKS 1
#include "vorbis/vorbisfile.h"
#undef	OV_EXCLUDE_STATIC_CALLBACKS


/**
 * write the page p into the given file pointer fp.
 *
 * return 0 on success and -1 on error.
 */
int
write_page(ogg_page *p, FILE *fp)
{

	if (fwrite(p->header, 1, p->header_len, fp) != p->header_len)
		return -1;
	if (fwrite(p->body, 1, p->body_len, fp) != p->body_len)
		return -1;
	return 0;
}


/*
 * copy a ogg/vorbis file from path_in to path_out, using the given Vorbis Comments
 * vc_out for the new file.
 *
 * return 0 on success and -1 on error.
 */
int
save_it(const char *path_in, struct vorbis_comment *vc_out, const char *path_out)
{
	FILE             *fp_in  = NULL;  /* input file pointer */
	FILE             *fp_out = NULL; /* output file pointer */
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
	enum {
		BUILDING_VC_PACKET, SETUP, B_O_S, START_READING,
		STREAMS_INITIALIZED, READING_HEADERS, READING_DATA,
		READING_DATA_NEED_FLUSH, READING_DATA_NEED_PAGEOUT, E_O_S,
		WRITE_FINISH, DONE_SUCCESS,
	} state;

	/*
	 * In order to modify the file's tag, we have to rewrite the entire
	 * file. The Ogg container is divided into "pages" and "packets" and the
	 * algorithm is to replace the SECOND ogg packet (which contains vorbis
	 * comments) and copy ALL THE OTHERS. See "Metadata workflow":
	 * https://xiph.org/vorbis/doc/libvorbis/overview.html
	 */

	state = BUILDING_VC_PACKET;
	/* create the packet holding our vorbis_comment */
	if (vorbis_commentheader_out(vc_out, &my_vc_packet) != 0)
		goto cleanup_label;

	state = SETUP;
	/* open files & stuff */
	(void)ogg_sync_init(&oy_in); /* always return 0 */
	if ((fp_in = fopen(path_in, "r")) == NULL)
		goto cleanup_label;
	if ((fp_out = (path_out == NULL ? stdout : fopen(path_out, "w"))) == NULL)
		goto cleanup_label;
	lastbs = granulepos = 0;

	nstream_in = 0;
bos_label: /* beginning of a stream */
	state = B_O_S; /* never read, but that's fine */
	nstream_in += 1;
	npage_in = npacket_in = 0;
	vorbis_info_init(&vi_in);
	vorbis_comment_init(&vc_in);

	state = START_READING;
	/* main loop: read the input file into buf in order to sync pages out */
	while (state != E_O_S) {
		switch (ogg_sync_pageout(&oy_in, &og_in)) {
		case 0:  /* more data needed or an internal error occurred. */
		case -1: /* stream has not yet captured sync (bytes were skipped). */
			if (feof(fp_in)) {
				if (state < READING_DATA)
					goto cleanup_label;
				/* There is no more data to read and we could
				   not get a page so we're done here. */
				state = E_O_S;
			} else {
				/* read more data and try again to get a page. */
				char *buf;
				size_t s;
				/* get a buffer */
				if ((buf = ogg_sync_buffer(&oy_in, BUFSIZ)) == NULL)
					goto cleanup_label;
				/* read a part of the file */
				if ((s = fread(buf, sizeof(char), BUFSIZ, fp_in)) == -1)
					goto cleanup_label;
				/* tell ogg how much was read */
				if (ogg_sync_wrote(&oy_in, s) == -1)
					goto cleanup_label;
			}
			continue;
		}
		/* here ogg_sync_pageout() returned 1 and a page was sync'ed. */
		if (++npage_in == 1) {
			/* init both input and output streams with the serialno
			   of the first page */
			if (ogg_stream_init(&os_in, ogg_page_serialno(&og_in)) == -1)
				goto cleanup_label;
			if (ogg_stream_init(&os_out, ogg_page_serialno(&og_in)) == -1) {
				ogg_stream_clear(&os_in);
				goto cleanup_label;
			}
			state = STREAMS_INITIALIZED;
		}

		/* put the page in input stream, and then loop through each its
		   packet(s) */
		if (ogg_stream_pagein(&os_in, &og_in) == -1)
			goto cleanup_label;
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
				 * will be used later. vc_in will not be unused.
				 */
				if (vorbis_synthesis_headerin(&vi_in, &vc_in, &op_in) != 0)
					goto cleanup_label;
				/* force a flush after the third ogg_packet */
				state = (npacket_in == 3 ? READING_DATA_NEED_FLUSH : READING_HEADERS);
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
				if (state == READING_DATA_NEED_FLUSH) {
					while (ogg_stream_flush(&os_out, &og_out)) {
						if (write_page(&og_out, fp_out) == -1)
							goto cleanup_label;
					}
				} else if (state == READING_DATA_NEED_PAGEOUT) {
					while (ogg_stream_pageout(&os_out, &og_out)) {
						if (write_page(&og_out, fp_out) == -1)
							goto cleanup_label;
					}
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
				 * vcedit and I fail to understand how
				 * granulepos could mismatch because we don't
				 * change the data packet.
				 */
				state = READING_DATA;
				if (op_in.granulepos == -1) {
					op_in.granulepos = granulepos;
				} else if (granulepos <= op_in.granulepos) {
					state = READING_DATA_NEED_PAGEOUT;
				} else /* if granulepos > op_in.granulepos */ {
					state = READING_DATA_NEED_FLUSH;
					granulepos = op_in.granulepos;
				}
			}
			/* insert the target packet into the output stream */
			if (ogg_stream_packetin(&os_out, target) == -1)
				goto cleanup_label;
		}
		if (ogg_page_eos(&og_in)) {
			/* og_in was the last page of the stream */
			state = E_O_S;
		}
	}

	/* forces remaining packets into a last page */
	os_out.e_o_s = 1;
	while (ogg_stream_flush(&os_out, &og_out)) {
		if (write_page(&og_out, fp_out) == -1)
			goto cleanup_label;
	/* ogg_page and ogg_packet structs always point to storage in libvorbis.
	   They're never freed or manipulated directly */

	/* check if we need to read another stream */
	if (!feof(fp_in)) {
		ogg_stream_clear(&os_in);
		ogg_stream_clear(&os_out);
		vorbis_comment_clear(&vc_in);
		vorbis_info_clear(&vi_in);
		goto bos_label;
	} else {
		(void)fclose(fp_in);
		fp_in = NULL;
	}

	state = WRITE_FINISH;
	if (fp_out != stdout && fclose(fp_out) != 0)
		goto cleanup_label;
	fp_out = NULL;
	state = DONE_SUCCESS;
	/* FALLTHROUGH */
cleanup_label:
	if (state >= STREAMS_INITIALIZED) {
		ogg_stream_clear(&os_in);
		ogg_stream_clear(&os_out);
	}
	if (state >= START_READING) {
		vorbis_comment_clear(&vc_in);
		vorbis_info_clear(&vi_in);
	}
	ogg_sync_clear(&oy_in);
	if (fp_out != stdout && fp_out != NULL)
		(void)fclose(fp_out);
	if (fp_in != NULL)
		(void)fclose(fp_in);
	ogg_packet_clear(&my_vc_packet);

	return (state == DONE_SUCCESS ? 0 : -1);
}


int
main(int argc, char **argv)
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
		return (EXIT_FAILURE);
	}

	/* try to read path_in as an ogg/vorbis file */
	if ((i = ov_fopen(path_in, &vf) != 0)) {
		(void)fprintf(stderr, "%s: can't open as ogg/vorbis file.\n", path_in);
		return (EXIT_FAILURE);
	}
	/* get the vorbis comments */
	if ((vc = ov_comment(&vf, -1)) == NULL) {
		(void)fprintf(stderr, "ov_comment\n");
		return (EXIT_FAILURE);
	}

	/* change something */
	vorbis_comment_add(vc, "test=42");

	/* display vorbis comments to stderr (in case stdin is used as output) */
	for (i = 0; i < vc->comments; i++)
		(void)fprintf(stderr, "%s\n", vc->user_comments[i]);

	/* now save the modified comments (and copy audio data) into path_out */
	if (save_it(path_in, vc, path_out) == -1) {
		(void)fprintf(stderr, "save_it failed.\n");
		return (EXIT_FAILURE);
	}

	/* cleanup */
	ov_clear(&vf);
	return (EXIT_SUCCESS);
}
