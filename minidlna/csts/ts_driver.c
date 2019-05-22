#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "ts_driver.h"

#define MAP_SIZE 		(64*1024)
#define MAP_MASK 		(MAP_SIZE-1)

#define TS_MAP_ADDRESS 	0xf0080000

#define	G2_TS_MAX_PID_ENTRY			(32)	/* Total pid entries for each RX queue. */
#define	G2_TS_MAX_CHANNEL			(6)		/* Maximum channel number of TS module. */

#define FATAL do { fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", \
__LINE__, __FILE__, errno, strerror(errno)); exit(1); } while(0)

#define TS_RXPID_CONTROL				0x00000400
#define TS_RXPID_GOOD_PKTCNT			0x00000418
#define TS_RXPID_ENTRY0_WORD0           0x0000042c
#define TS_RXPID_ENTRY0_WORD1           0x00000430

typedef volatile union {
  struct {
#   ifdef CS_BIG_ENDIAN
    cs_uint32 rsrvd3               :  7 ;
    cs_uint32 rx_packet_len        :  9 ; /* bits 24:16 */
    cs_uint32 rsrvd2               :  4 ;
    cs_uint32 default_qid          :  4 ; /* bits 11:8 */
    cs_uint32 rsrvd1               :  1 ;
    cs_uint32 continuous_mode      :  1 ; /* bits 6:6 */
    cs_uint32 data_mode            :  1 ; /* bits 5:5 */
    cs_uint32 endian_dir           :  1 ; /* bits 4:4 */
    cs_uint32 bit_dir              :  1 ; /* bits 3:3 */
    cs_uint32 start_pulse          :  1 ; /* bits 2:2 */
    cs_uint32 pid_enable           :  1 ; /* bits 1:1 */
    cs_uint32 rx_enable            :  1 ; /* bits 0:0 */
#   else /* CS_LITTLE_ENDIAN */
    cs_uint32 rx_enable            :  1 ; /* bits 0:0 */
    cs_uint32 pid_enable           :  1 ; /* bits 1:1 */
    cs_uint32 start_pulse          :  1 ; /* bits 2:2 */
    cs_uint32 bit_dir              :  1 ; /* bits 3:3 */
    cs_uint32 endian_dir           :  1 ; /* bits 4:4 */
    cs_uint32 data_mode            :  1 ; /* bits 5:5 */
    cs_uint32 continuous_mode      :  1 ; /* bits 6:6 */
    cs_uint32 rsrvd1               :  1 ;
    cs_uint32 default_qid          :  4 ; /* bits 11:8 */
    cs_uint32 rsrvd2               :  4 ;
    cs_uint32 rx_packet_len        :  9 ; /* bits 24:16 */
    cs_uint32 rsrvd3               :  7 ;
#   endif
  } bf ;
  cs_uint32     wrd ;
} TS_RXPID_CONTROL_t;

typedef volatile union {
  struct {
#   ifdef CS_BIG_ENDIAN
    cs_uint32 valid0               :  1 ; /* bits 31:31 */
    cs_uint32 action0              :  1 ; /* bits 30:30 */
    cs_uint32 new_pid0             : 13 ; /* bits 29:17 */
    cs_uint32 qid0                 :  4 ; /* bits 16:13 */
    cs_uint32 pid                  : 13 ; /* bits 12:0 */
#   else /* CS_LITTLE_ENDIAN */
    cs_uint32 pid                  : 13 ; /* bits 12:0 */
    cs_uint32 qid0                 :  4 ; /* bits 16:13 */
    cs_uint32 new_pid0             : 13 ; /* bits 29:17 */
    cs_uint32 action0              :  1 ; /* bits 30:30 */
    cs_uint32 valid0               :  1 ; /* bits 31:31 */
#   endif
  } bf ;
  cs_uint32     wrd ;
} TS_RXPID_ENTRY0_WORD0_t;

typedef volatile union {
  struct {
#   ifdef CS_BIG_ENDIAN
    cs_uint32 rsrvd1               : 13 ;
    cs_uint32 valid1               :  1 ; /* bits 18:18 */
    cs_uint32 action1              :  1 ; /* bits 17:17 */
    cs_uint32 new_pid1             : 13 ; /* bits 16:4 */
    cs_uint32 qid1                 :  4 ; /* bits 3:0 */
#   else /* CS_LITTLE_ENDIAN */
    cs_uint32 qid1                 :  4 ; /* bits 3:0 */
    cs_uint32 new_pid1             : 13 ; /* bits 16:4 */
    cs_uint32 action1              :  1 ; /* bits 17:17 */
    cs_uint32 valid1               :  1 ; /* bits 18:18 */
    cs_uint32 rsrvd1               : 13 ;
#   endif
  } bf ;
  cs_uint32     wrd ;
} TS_RXPID_ENTRY0_WORD1_t;

#define g2_ts_read_reg(addr)		(*(volatile u32 *)(ts_base_addr + addr))
#define g2_ts_write_reg(addr, val)	(*(volatile u32 *)(ts_base_addr + addr)= val)

static cs_uint32 	ts_base_addr;

static unsigned long ts_good_packet_counter = 0;

cs_int32 g2_ts_mmap_init(void)
{
	cs_int32	fd;
	void 		*pts_addr;

	/* Must be O_SYNC, non cache */
	if((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) FATAL;

	fflush(stdout);

	pts_addr = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,\
		    fd,TS_MAP_ADDRESS & ~MAP_MASK);

    if(pts_addr == (void *) -1) {
	  	printf("map_base:%x \n",pts_addr);
		close(fd);
	  	FATAL;
	}
//	printf("Memory mapped at address %p.\n", pts_addr);

    ts_base_addr = (unsigned int)(pts_addr + (TS_MAP_ADDRESS & MAP_MASK));

	return (fd);
}

cs_int32 g2_ts_mmap_exit(cs_int32 fd)
{
	close(fd);

	return (0);
}

cs_int32 g2_ts_replace_pid(cs_uint8	ch_id, cs_uint16 org_pid, cs_uint16 new_pid)
{
	TS_RXPID_ENTRY0_WORD0_t	word0;
	cs_uint32 				i;

	if (org_pid == new_pid) {
		return (-1);
	}

	for (i=0; i<G2_TS_MAX_PID_ENTRY; i++) {
		word0.wrd = g2_ts_read_reg(TS_RXPID_ENTRY0_WORD0 + ch_id*0x200 + i*8);
		printf("%s : read TS_RXPID_ENTRY0_WORD0 = %x \n",__func__,word0.wrd);
		if ((word0.bf.valid0 == 1) && (word0.bf.pid==org_pid)) {
			word0.bf.new_pid0 = new_pid;
			word0.bf.action0 = 1;
			printf("%s : write TS_RXPID_ENTRY0_WORD0 = %x \n",__func__,word0.wrd);
			g2_ts_write_reg(TS_RXPID_ENTRY0_WORD0 + ch_id*0x200 + i*8, word0.wrd);
			break;
		}
    }

    if (i < G2_TS_MAX_PID_ENTRY) {
    	return (0);
    }
    else {
		return (-1);
	}
}

cs_int32 g2_ts_duplicate_pid(cs_uint8 ch_id, cs_uint16 pid, cs_uint8 qid, cs_uint16 new_pid)
{
	TS_RXPID_ENTRY0_WORD0_t	word0;
	TS_RXPID_ENTRY0_WORD1_t	word1;
	cs_uint32				i;

	for (i=0; i<G2_TS_MAX_PID_ENTRY; i++) {
		word0.wrd = g2_ts_read_reg(TS_RXPID_ENTRY0_WORD0 + ch_id*0x200 + i*8);
		word1.wrd = g2_ts_read_reg(TS_RXPID_ENTRY0_WORD1 + ch_id*0x200 + i*8);
		printf("%s : read TS_RXPID_ENTRY0_WORD0 = %x WORD1 = %x \n",__func__,word0.wrd,word1.wrd);
		if ((word0.bf.valid0 == 1) && (word0.bf.pid==pid)) {
			word1.bf.qid1 = qid;
			word1.bf.valid1 = 1;
			if (pid != new_pid) {
				word1.bf.new_pid1 = new_pid;
				word1.bf.action1 = 1;
			}
			g2_ts_write_reg(TS_RXPID_ENTRY0_WORD0 + ch_id*0x200 + i*8, word0.wrd);
			g2_ts_write_reg(TS_RXPID_ENTRY0_WORD1 + ch_id*0x200 + i*8, word1.wrd);
			break;
		}
    }

    if (i < G2_TS_MAX_PID_ENTRY) {
    	return (0);
    }
    else {
		return (-1);
	}
}

cs_int32 g2_ts_add_pid(cs_uint8	ch_id, cs_uint16 pid)
{
	TS_RXPID_ENTRY0_WORD0_t	word0;
	TS_RXPID_ENTRY0_WORD1_t	word1;
	cs_uint32				i;

	for (i=0; i<G2_TS_MAX_PID_ENTRY; i++) {
		word0.wrd = g2_ts_read_reg(TS_RXPID_ENTRY0_WORD0 + ch_id*0x200 + i*8);
		word1.wrd = g2_ts_read_reg(TS_RXPID_ENTRY0_WORD1 + ch_id*0x200 + i*8);
		printf("%s : read TS_RXPID_ENTRY0_WORD0 = %x WORD1 = %x \n",__func__,word0.wrd,word1.wrd);
		if (word0.bf.valid0 == 0) {
			word0.bf.pid 	= pid;
			word0.bf.qid0 	= ch_id;
			word0.bf.valid0	= 1;
			g2_ts_write_reg(TS_RXPID_ENTRY0_WORD0 + ch_id*0x200 + i*8, word0.wrd);
			g2_ts_write_reg(TS_RXPID_ENTRY0_WORD1 + ch_id*0x200 + i*8, word1.wrd);
			break;
		}
    }

    if (i < G2_TS_MAX_PID_ENTRY) {
    	return (0);
    }
    else {
		return (-1);
	}
}

cs_int32 g2_ts_del_pid(cs_uint8	ch_id, cs_uint16 pid)
{
	TS_RXPID_ENTRY0_WORD0_t	word0;
	cs_uint32				i;

	for (i=0; i<G2_TS_MAX_PID_ENTRY; i++) {
		word0.wrd = g2_ts_read_reg(TS_RXPID_ENTRY0_WORD0 + ch_id*0x200 + i*8);
		printf("%s : read TS_RXPID_ENTRY0_WORD0 = %x \n",__func__,word0.wrd);
		if ((word0.bf.valid0 == 1) && (word0.bf.pid==pid)) {
			word0.wrd = 0;
			printf("%s : write TS_RXPID_ENTRY0_WORD0 = %x \n",__func__,word0.wrd);
			g2_ts_write_reg(TS_RXPID_ENTRY0_WORD0 + ch_id*0x200 + i*8, word0.wrd);
			break;
		}
    }

    if (i < G2_TS_MAX_PID_ENTRY) {
    	return (0);
    }
    else {
		return (-1);
	}
}

cs_int32 g2_ts_flush_all_pid(cs_uint8 ch_id)
{
		cs_uint32 i;

		if ((ch_id >= G2_TS_MAX_CHANNEL) && (ch_id < G2_TS_MAX_CHANNEL*2)){
			 ch_id = ch_id % G2_TS_MAX_CHANNEL;
				for (i=0; i<G2_TS_MAX_PID_ENTRY; i++) {
						g2_ts_write_reg(TS_RXPID_ENTRY0_WORD1 + ch_id*0x200 + i*8, 0x00000000);
				}				
		}
		else {
				for (i=0; i<G2_TS_MAX_PID_ENTRY; i++) {
						g2_ts_write_reg(TS_RXPID_ENTRY0_WORD0 + ch_id*0x200 + i*8, 0x00000000);
						g2_ts_write_reg(TS_RXPID_ENTRY0_WORD1 + ch_id*0x200 + i*8, 0x00000000);
			}
		}
		return (0);
}

cs_uint32 g2_ts_get_statistic(cs_uint8 ch_id)
{
	ts_good_packet_counter += g2_ts_read_reg(TS_RXPID_GOOD_PKTCNT + ch_id*0x200);

	return (ts_good_packet_counter);
}

cs_int32 g2_ts_clear_statistic(cs_uint8 ch_id)
{
	ts_good_packet_counter += g2_ts_read_reg(TS_RXPID_GOOD_PKTCNT + ch_id*0x200);
	ts_good_packet_counter = 0;

	return (0);
}

cs_int32 g2_ts_set_length(cs_uint8 ch_id, cs_uint16 length)
{
	TS_RXPID_CONTROL_t	rxpid_ctrl;

	rxpid_ctrl.wrd = g2_ts_read_reg(TS_RXPID_CONTROL + ch_id*0x200);
	rxpid_ctrl.bf.rx_packet_len = length;
	g2_ts_write_reg(TS_RXPID_CONTROL + ch_id*0x200, rxpid_ctrl.wrd);
	printf("%s : rxpid_ctrl.wrd = %x  \n",__func__,rxpid_ctrl.wrd);
	return (0);
}

cs_int32 g2_ts_set_rx_enable(cs_uint8 ch_id, cs_uint8 mode)
{
	TS_RXPID_CONTROL_t	rxpid_ctrl;

	rxpid_ctrl.wrd = g2_ts_read_reg(TS_RXPID_CONTROL + ch_id*0x200);
	rxpid_ctrl.bf.rx_enable = mode;
	g2_ts_write_reg(TS_RXPID_CONTROL + ch_id*0x200, rxpid_ctrl.wrd);
	printf("%s : rxpid_ctrl.wrd = %x  \n",__func__,rxpid_ctrl.wrd);
	return (0);
}


#if 0
void usage(void)
{
	fprintf(stderr, "Usage: ts_config - configure parameters to TS controller registers.\n");
	fprintf(stderr, "\t-a ch_id pid [qid] [new_pid] : add a new PID.\n");
    fprintf(stderr, "\t-c ch_id                     : clear good packets statistic.\n");
	fprintf(stderr, "\t-d ch_id pid                 : delete a PID.\n");
    fprintf(stderr, "\t-e ch_id mode                : set TS RX enable/disable.\n");
	fprintf(stderr, "\t-f ch_id                     : flush all PIDs.\n");
    fprintf(stderr, "\t-l ch_id length              : set TS packet length.\n");
	fprintf(stderr, "\t-r ch_id org_pid new_pid     : replace org_pid with new_pid.\n");
    fprintf(stderr, "\t-s ch_id                     : get good packets statistic.\n");
	exit(1);
}

/* ts_config -r ch_id org_pid new_pid */
int ts_replace_pid(int argc, char **argv)
{
	unsigned short			ch_id;
	unsigned short			org_pid;
	unsigned short			new_pid;
	cs_int32			ret;

	if (argc != 5) {
    	fprintf(stderr,"Usage : ts_config -r ch_id org_pid new_pid\n");
    	exit(1);
    }

    ch_id   = strtoul(argv[2],NULL, 0);
    org_pid = strtoul(argv[3],NULL, 0);
    new_pid = strtoul(argv[4],NULL, 0);
    printf("%s : ch = %d, org_pid = %d new_pid = %d \n",__func__,ch_id,org_pid,new_pid);

	ret = g2_ts_replace_pid(ch_id,org_pid,new_pid);

	return (ret);
}

/* ts_config -a ch_id pid [qid] [new_pid] */
int ts_add_pid(int argc, char **argv)
{
	TS_RXPID_ENTRY0_WORD0_t	word0;
	TS_RXPID_ENTRY0_WORD1_t	word1;
	unsigned short			ch_id;
	unsigned short			pid;
	unsigned short			qid;
	unsigned short			new_pid;
	unsigned int			i;

	if (argc == 4) {
	    ch_id   = strtoul(argv[2],NULL, 0);
	    pid 	= strtoul(argv[3],NULL, 0);
	}
	else if (argc == 5) {
	    qid 	= strtoul(argv[4],NULL, 0);
	}
	else if (argc == 6) {
	    new_pid = strtoul(argv[5],NULL, 0);
	}
	else {
    	fprintf(stderr,"Usage : ts_config -a ch_id pid [qid] [new_pid]\n");
    	exit(1);
    }

	for (i=0; i<G2_TS_MAX_PID_ENTRY; i++) {
		word0.wrd = g2_ts_read_reg(TS_RXPID_ENTRY0_WORD0 + ch_id*0x200 + i*8);
		word1.wrd = g2_ts_read_reg(TS_RXPID_ENTRY0_WORD1 + ch_id*0x200 + i*8);
		printf("%s : read TS_RXPID_ENTRY0_WORD0 = %x WORD1 = %x \n",__func__,word0.wrd,word1.wrd);
		if (word0.bf.valid0 == 0) {
			word0.bf.pid 	= pid;
			word0.bf.qid0 	= ch_id;
			word0.bf.valid0	= 1;
			if (argc == 5) {
				word1.bf.qid1 = qid;
				word1.bf.valid1 = 1;
			}
			else if (argc == 6) {
				word1.bf.new_pid1 = new_pid;
				word1.bf.action1 = 1;
				word1.bf.valid1 = 1;
			}
			printf("%s : read TS_RXPID_ENTRY0_WORD0 = %x WORD1 = %x \n",__func__,word0.wrd,word1.wrd);
			g2_ts_write_reg(TS_RXPID_ENTRY0_WORD0 + ch_id*0x200 + i*8, word0.wrd);
			g2_ts_write_reg(TS_RXPID_ENTRY0_WORD1 + ch_id*0x200 + i*8, word1.wrd);
			break;
		}
    }

    if (i < G2_TS_MAX_PID_ENTRY) {
    	return (0);
    }
    else {
		return (-1);
	}
}

/* ts_config -d ch_id pid */
int ts_del_pid(int argc, char **argv)
{
	unsigned short			ch_id;
	unsigned short			pid;
	cs_int32			ret;

	if (argc == 4) {
	    ch_id   = strtoul(argv[2],NULL, 0);
	    pid 	= strtoul(argv[3],NULL, 0);
	}
	else {
    	fprintf(stderr,"Usage : ts_config -a ch_id pid \n");
    	exit(1);
    }

	ret = g2_ts_del_pid(ch_id, pid);

	return (ret);
}

/* ts_config -f ch_id */
int ts_flush_all_pid(int argc, char **argv)
{
	cs_uint16	ch_id;
	cs_int32	ret;

	if (argc == 3) {
	    ch_id   = strtoul(argv[2],NULL, 0);
	}
	else {
    	fprintf(stderr,"Usage : ts_config -f ch_id \n");
    	exit(1);
    }

	ret = g2_ts_flush_all_pid(ch_id);

	return (ret);
}

/* ts_config -s ch_id */
int ts_get_statistic(int argc, char **argv)
{
	cs_uint16		ch_id;
	unsigned long 	counter;

	if (argc == 3) {
	    ch_id   = strtoul(argv[2],NULL, 0);
	}
	else {
    	fprintf(stderr,"Usage : ts_config -s ch_id \n");
    	exit(1);
    }

	counter = g2_ts_get_statistic(ch_id);
	printf("Good packets counter = 0x%x \n",counter);

	return (counter);
}

/* ts_config -c ch_id */
int ts_clear_statistic(int argc, char **argv)
{
	cs_uint16		ch_id;
	unsigned long 	counter;

	if (argc == 3) {
	    ch_id   = strtoul(argv[2],NULL, 0);
	}
	else {
    	fprintf(stderr,"Usage : ts_config -c ch_id \n");
    	exit(1);
    }

	counter = g2_ts_clear_statistic(ch_id);

	return (counter);
}


/* ts_config -l ch_id length */
int ts_set_length(int argc, char **argv)
{
	cs_uint16	ch_id;
	cs_uint16	length;
	cs_int32	ret;

	if (argc == 4) {
	    ch_id   = strtoul(argv[2],NULL, 0);
	    length 	= strtoul(argv[3],NULL, 0);
	}
	else {
    	fprintf(stderr,"Usage : ts_config -l ch_id length \n");
    	exit(1);
    }

	ret = g2_ts_set_length(ch_id, length);

	return (ret);
}

/* ts_config -e ch_id mode */
int ts_set_rx_enable(int argc, char **argv)
{
	cs_uint16	ch_id;
	cs_uint16	mode;
	cs_int32	ret;

	if (argc == 4) {
	    ch_id   = strtoul(argv[2],NULL, 0);
	    mode 	= strtoul(argv[3],NULL, 0);
	}
	else {
    	fprintf(stderr,"Usage : ts_config -e ch_id mode \n");
    	exit(1);
    }

	ret = g2_ts_set_rx_enable(ch_id, mode);

	return (ret);
}

int main(int argc, char **argv)
{
	cs_int32	fd;
	cs_int32	ret=-1;

	if (argc < 2) {
		usage();
	}

	fd = g2_ts_mmap_init();


	if ((strcmp(argv[1], "-h")== 0) ||
	 	(strcmp(argv[1], "--help")== 0) ||
	 	(strcmp(argv[1], "-v")== 0) ||
	 	(strcmp(argv[1], "--version")== 0))
	{
		usage();
	}
	else if ((strcmp(argv[1], "-r")== 0) ||
			 (strcmp(argv[1], "--replace")== 0))
	{
		ret = ts_replace_pid(argc,argv);
	}
	else if ((strcmp(argv[1], "-a")== 0) ||
			 (strcmp(argv[1], "--add")== 0))
	{
		ret = ts_add_pid(argc,argv);
	}
	else if ((strcmp(argv[1], "-d")== 0) ||
			 (strcmp(argv[1], "--del")== 0) ||
			 (strcmp(argv[1], "--delete")== 0))
	{
		ret = ts_del_pid(argc,argv);
	}
	else if ((strcmp(argv[1], "-f")== 0) ||
			 (strcmp(argv[1], "--flush")== 0))
	{
		ret = ts_flush_all_pid(argc,argv);
	}
	else if ((strcmp(argv[1], "-s")== 0) ||
			 (strcmp(argv[1], "--statistic")== 0))
	{
		ret = ts_get_statistic(argc,argv);
	}
	else if ((strcmp(argv[1], "-c")== 0) ||
			 (strcmp(argv[1], "--clear")== 0))
	{
		ret = ts_clear_statistic(argc,argv);
	}
	else if ((strcmp(argv[1], "-l")== 0) ||
			 (strcmp(argv[1], "--length")== 0))
	{
		ret = ts_set_length(argc,argv);
	}
	else if ((strcmp(argv[1], "-e")== 0) ||
			 (strcmp(argv[1], "--enable")== 0))
	{
		ret = ts_set_rx_enable(argc,argv);
	}
	else if ((strcmp(argv[1], "-q")== 0) ||
			 (strcmp(argv[1], "--exit")== 0) ||
			 (strcmp(argv[1], "--quit")== 0))
	{
		exit(0);
	}

	g2_ts_mmap_exit(fd);
}
#endif