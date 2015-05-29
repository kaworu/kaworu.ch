/* found here: http://lists.xiph.org/pipermail/vorbis-dev/2010-November/020173.html */

/*!\file\brief Fixes ogg vorbis files.

A command-line tool that reads in a ogg vorbis file and recalculates
the granulepos markers that are indispensable for seeking and cutting
the file at the right place with mp3splt-gtk.
*/

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <vorbis/vorbisenc.h>
#include <vorbis/vorbisfile.h>
#include <ogg/ogg.h>

#ifdef _WIN32 /* We need the following two to set stdin/stdout to binary */
#include <io.h>
#include <fcntl.h>
#endif

#if defined(__MACOS__) && defined(__MWERKS__)
#include <console.h>      /* CodeWarrior's Mac "command-line" support */
#endif

ogg_int64_t granulepos;

void packetwrite
(
 ogg_stream_state *os_out,
 ogg_page *og_out,
 ogg_packet *op_out,
 FILE *outputfile
 )
{
  // Correct the packet
  op_out->granulepos=granulepos;
  
    /* Push the packet into the stream...*/
  ogg_stream_packetin(os_out,op_out);
  /* ...and write the stream to the output file.*/
  while(ogg_stream_flush(os_out,og_out))
    {
      fwrite(og_out->header,1,og_out->header_len,outputfile);
      fwrite(og_out->body,1,og_out->body_len,outputfile);
    }
}

// Heavily based on
// http://svn.xiph.org/trunk/vorbis/examples/decoder_example.c
// from Nov1 3, 2010
void oggfix(char *src,char *dest)
{
  ogg_sync_state   oy_in; /* sync and verify incoming physical bitstream */
  ogg_stream_state os_in; /* take physical pages, weld into a logical
                          stream of packets */
  ogg_stream_state os_out; /* take physical pages, weld into a logical
                          stream of packets */
  ogg_page         og_in; /* one Ogg bitstream page. Vorbis packets are inside */
  ogg_page         og_out; /* one Ogg bitstream page. Vorbis packets are inside */
  ogg_packet       op_in; /* one raw packet of data for decode */

  vorbis_info      vi_in; /* struct that stores all the static vorbis bitstream
                          settings */
  vorbis_comment   vc_in; /* struct that stores all the bitstream user comments */
  vorbis_dsp_state vd_in; /* central working state for the packet->PCM decoder */
  vorbis_block     vb_in; /* local working space for packet->PCM decode */

  char *buffer;


  int  bytes;

  FILE *inputfile;
  FILE *outputfile;

  granulepos=0;

  if((inputfile=fopen(src,"r"))==0)
    {
      fprintf (stderr,
	       "Error: Cannot open input file.\n");
      exit(-1);
    };

  if(dest==NULL)
    outputfile=stdout;
  else
    {
      if((outputfile=fopen(dest,"w"))==0)
      {
	fprintf (stderr,"Error: Cannot open output file.\n");
	exit(-1);
      }  
    }
  

  /********** Setup ************/

  /* Initialize breaking the stream into pages */
  ogg_sync_init(&oy_in);
      
  /* Since an ogg stream can be followed by another (and so on) we now
     go into an endless loop that decodes all streams this file contains.*/
  while(1){
    int eos=0;
    int i;

    /* grab some data at the head of the stream. We want the first page
       (which is guaranteed to be small and only contain the Vorbis
       stream initial header) We need the first page to get the stream
       serialno. */

    /* submit a 4k block to libvorbis' Ogg layer */
    buffer=ogg_sync_buffer(&oy_in,4096);
    bytes=fread(buffer,1,4096,inputfile);
    ogg_sync_wrote(&oy_in,bytes);
    
    /* Get the first page. */
    if(ogg_sync_pageout(&oy_in,&og_in)!=1){
      /* have we simply run out of data?  If so, we're done and this
	 was the last stream in the file. */
      if(bytes<4096)break;
      
      /* error case.  Must not be Vorbis data */
      fprintf(stderr,"Input does not appear to be an Ogg bitstream.\n");
      exit(1);
    }
  
    /* Get the serial number and set up the rest of decode. */
    /* serialno first; use it to set up a logical stream */
    ogg_stream_init(&os_in,ogg_page_serialno(&og_in));
    
    /* Initialize the output stream. Since it is a corrected version
       of the old one the serial number of the old stream is kept*/
    ogg_stream_init(&os_out,ogg_page_serialno(&og_in));
 
    /* extract the initial header from the first page and verify that the
       Ogg bitstream is in fact Vorbis data */
    
    /* I handle the initial header first instead of just having the code
       read all three Vorbis headers at once because reading the initial
       header is an easy way to identify a Vorbis bitstream and it's
       useful to see that functionality seperated out. */
    
    vorbis_info_init(&vi_in);
    vorbis_comment_init(&vc_in);
    if(ogg_stream_pagein(&os_in,&og_in)<0){ 
      /* error; stream version mismatch perhaps */
      fprintf(stderr,"Error reading first page of Ogg bitstream data.\n");
      exit(1);
    }
    
    if(ogg_stream_packetout(&os_in,&op_in)!=1){ 
      /* no page? must not be vorbis */
      fprintf(stderr,"Error reading initial header packet.\n");
      exit(1);
    }
    // Write out the Ogg header
    packetwrite(&os_out,&og_out,&op_in,outputfile);

    if(vorbis_synthesis_headerin(&vi_in,&vc_in,&op_in)<0){ 
      /* error case; not a vorbis header */
      fprintf(stderr,"This Ogg bitstream does not contain Vorbis "
              "audio data.\n");
      exit(1);
    }
    
    /* At this point, we're sure we're Vorbis. We've set up the logical
       (Ogg) bitstream decoder. Get the comment and codebook headers and
       set up the Vorbis decoder */
    
    /* The next two packets in order are the comment and codebook headers.
       They're likely large and may span multiple pages. Thus we read
       and submit data until we get our two packets, watching that no
       pages are missing. If a page is missing, error out; losing a
       header page is the only place where missing data is fatal. */
    
    i=0;
    while(i<2){
      while(i<2){
        int result=ogg_sync_pageout(&oy_in,&og_in);
        if(result==0)break; /* Need more data */
        /* Don't complain about missing or corrupt data yet. We'll
           catch it at the packet output phase */
        if(result==1){
          ogg_stream_pagein(&os_in,&og_in); /* we can ignore any errors here
                                         as they'll also become apparent
                                         at packetout */
          while(i<2){
            result=ogg_stream_packetout(&os_in,&op_in);
            if(result==0)break;
            if(result<0){
              /* Uh oh; data at some point was corrupted or missing!
                 We can't tolerate that in a header.  Die. */
              fprintf(stderr,"Corrupt secondary header.  Exiting.\n");
              exit(1);
            }
            result=vorbis_synthesis_headerin(&vi_in,&vc_in,&op_in);
            if(result<0){
              fprintf(stderr,"Corrupt secondary header.  Exiting.\n");
              exit(1);
            }
	    // Copy this Vorbis header packet
	    packetwrite(&os_out,&og_out,&op_in,outputfile);
            i++;
          }
        }
      }
      /* no harm in not checking before adding more */
      buffer=ogg_sync_buffer(&oy_in,4096);
      bytes=fread(buffer,1,4096,inputfile);
      if(bytes==0 && i<2){
        fprintf(stderr,"End of file before finding all Vorbis headers!\n");
        exit(1);
      }
      ogg_sync_wrote(&oy_in,bytes);
    }
    
    /* Initialize the Vorbis
       packet->PCM decoder. */
    if(vorbis_synthesis_init(&vd_in,&vi_in)==0){ /* central decode state */
      vorbis_block_init(&vd_in,&vb_in);          /* local state for most of the decode
                                              so multiple block decodes can
                                              proceed in parallel. We could init
                                              multiple vorbis_block structures
                                              for vd here */
      
      /* The rest is just a straight decode loop until end of stream */
      while(!eos){
        while(!eos){
          int result=ogg_sync_pageout(&oy_in,&og_in);
          if(result==0)break; /* need more data */
          if(result<0){ /* missing or corrupt data at this page position */
            fprintf(stderr,"Corrupt or missing data in bitstream; "
                    "continuing...\n");
          }else{
            ogg_stream_pagein(&os_in,&og_in); /* can safely ignore errors at
                                           this point */
            while(1){
              result=ogg_stream_packetout(&os_in,&op_in);
              
              if(result==0)break; /* need more data */
              if(result<0){ /* missing or corrupt data at this page position */
                /* no reason to complain; already complained above */
              }else{
                /* we have a packet.  Decode it */
                float **pcm;
                int samples;
                
                if(vorbis_synthesis(&vb_in,&op_in)==0) /* test for success! */
                  vorbis_synthesis_blockin(&vd_in,&vb_in);
                /* 
		   Now decode the current ogg packets just to be able
		   to look how long this packet really is.
                 */
                while((samples=vorbis_synthesis_pcmout(&vd_in,&pcm))>0){
		  granulepos+=samples;
		  /* tell libvorbis how many samples we actually consumed */
		  vorbis_synthesis_read(&vd_in,samples);
                }

		/* Copy this packet to the output stream. The
		   packetwrite function makes sure the packet is fixed
		   before being written.*/
		packetwrite(&os_out,&og_out,&op_in,outputfile);
		
	      }
            }
            if(ogg_page_eos(&og_in))eos=1;
          }
        }
        if(!eos){
          buffer=ogg_sync_buffer(&oy_in,4096);
          bytes=fread(buffer,1,4096,inputfile);
          ogg_sync_wrote(&oy_in,bytes);
          if(bytes==0)eos=1;
        }
      }
      
      /* ogg_page and ogg_packet structs always point to storage in
         libvorbis.  They're never freed or manipulated directly */
      
      vorbis_block_clear(&vb_in);
      vorbis_dsp_clear(&vd_in);
    }else{
      fprintf(stderr,"Error: Corrupt header during playback initialization.\n");
    }

    /* clean up this logical bitstream; before exit we see if we're
       followed by another [chained] */
    
    ogg_stream_clear(&os_in);
    ogg_stream_clear(&os_out);
    vorbis_comment_clear(&vc_in);
    vorbis_info_clear(&vi_in);  /* must be called last */
  }
  
  /* OK, clean up the framer */
  ogg_sync_clear(&oy_in);
  
  // close the files
  fclose(inputfile);
  if(dest!=NULL)
    fclose(outputfile);
}


int main (int argc, char **argv)
{
  char *outputdir=NULL;
  int index;
  int c;
  char *outputfilename;

#ifdef _WIN32 /* We need to set stdin/stdout to binary mode. */
  _setmode( _fileno( stdin ), _O_BINARY );
  _setmode( _fileno( stdout ), _O_BINARY );
#endif


  opterr = 0;
  
  while ((c = getopt (argc, argv, "d:")) != -1)
    switch (c)
      {
      case 'd':
	outputdir = optarg;
	mkdir(outputdir,0777);
	fprintf (stderr, "Setting the output directory to %s.\n", optarg);
	break;
      case '?':
	if (optopt == 'c')
	  fprintf (stderr, "Option -%c requires an argument.\n", optopt);
	else if (isprint (optopt))
	  fprintf (stderr, "Unknown option `-%c'.\n", optopt);
	else
	  fprintf (stderr,
		   "Unknown option character `\\x%x'.\n",
		   optopt);
	return 1;
      default:
	abort ();
      }

  if (optind == argc) 
    {
      fprintf (stderr,
	       "Error: No file name given.\n\nUsage:\n");
      fprintf (stderr,
	       "For to fix an ogg file and output it on stdout:\n");
      fprintf (stderr,
	       "%s <filename>\n\n",argv[0]);
      fprintf (stderr,
	       "For to fix a bunch of ogg files and to output them into a directory:\n");
      fprintf (stderr,
	       "%s -d <dirname> <filename> <filename> ...\n",argv[0]);
      return 1;
    }
  
  if(outputdir == NULL)
    {
      if (optind != argc-1)
	{
	  fprintf (stderr,
		   "Error: More than one file name given (%i) and no outputdir.\n",
		   optind - argc);
	  return 1;
	} else
	oggfix(argv[optind],NULL);
    }
  else
    {
      for (index = optind; index < argc; index++)
	{
	  char *basename_pos;

	  /* Find the bae name of the input file with out directory
	  part: A file name beginning with ../ may otherwise lead to
	  writing data somewhere we don't want to.*/
	  basename_pos= argv[optind];
	  while(strchr(basename_pos,'/')!=basename_pos)
	    basename_pos=strchr(basename_pos,'/');

	  outputfilename=malloc(strlen(basename_pos)+strlen(outputdir)+2);
	  strcpy(outputfilename,outputdir);
	  strcat(outputfilename,"/");
	  strcat(outputfilename,basename_pos);

	  fprintf (stderr,
		   "%i of %i: %s => %s.\n",
		   index-optind+1, argc -optind ,argv[index],outputfilename);
	  
	  oggfix(argv[index],outputfilename);
	  free(outputfilename);
	}
    }
  return 0;
}

