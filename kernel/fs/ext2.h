/*
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/include/linux/minix_fs.h
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#ifndef _EXT2_H_
#define _EXT2_H_

#include <fs.h>
#include <stdint.h>
#include <sys/types.h>
#include <lib/spinlock.h>
#include <lib/mutex.h>
#include <lib/linked_list.h>
#include <memory>

namespace fs
{
namespace ext2
{

/* XXX Here for now... not interested in restructing headers JUST now */

/* data type for block offset of block group */
typedef int32_t grpblk_t;

/* data type for filesystem-wide blocks number */
typedef uint32_t fsblk_t;

#define E2FSBLK "%lu"

struct reserve_window {
    fsblk_t		_rsv_start;	/* First byte reserved */
    fsblk_t		_rsv_end;	/* Last byte reserved or 0 */
};

struct reserve_window_node {
    uint32_t			rsv_goal_size;
    uint32_t			rsv_alloc_hit;
    struct reserve_window	rsv_window;
};

struct block_alloc_info {
    /* information about reservation window */
    struct reserve_window_node	rsv_window_node;
    /*
     * was i_next_alloc_block in inode_info
     * is the logical (file-relative) number of the
     * most-recently-allocated block in this file.
     * We use this for detecting linearly ascending allocation requests.
     */
    uint32_t			last_alloc_logical_block;
    /*
     * Was i_next_alloc_goal in inode_info
     * is the *physical* companion to i_next_alloc_block.
     * it the the physical block number of the block which was most-recentl
     * allocated to this file.  This give us the goal (target) for the next
     * allocation when we detect linearly ascending requests.
     */
    fsblk_t		last_alloc_physical_block;
};

#define rsv_start rsv_window._rsv_start
#define rsv_end rsv_window._rsv_end

struct superblock;

/*
 * second extended-fs super-block data in memory
 */
struct sb_info  : fs::superblock
{
    unsigned long s_frag_size;	/* Size of a fragment in bytes */
    unsigned long s_frags_per_block;/* Number of fragments per block */
    unsigned long s_inodes_per_block;/* Number of inodes per block */
    unsigned long s_frags_per_group;/* Number of fragments in a group */
    unsigned long s_blocks_per_group;/* Number of blocks in a group */
    unsigned long s_inodes_per_group;/* Number of inodes in a group */
    unsigned long s_itb_per_group;	/* Number of inode table blocks per group */
    unsigned long s_gdb_count;	/* Number of group descriptor blocks */
    unsigned long s_desc_per_block;	/* Number of group descriptors per block */
    unsigned long s_groups_count;	/* Number of groups in the fs */
    unsigned long s_overhead_last;  /* Last calculated overhead */
    unsigned long s_blocks_last;    /* Last seen block count */
    std::unique_ptr<superblock> s_es;	/* Pointer to the super block */
    unsigned long  s_mount_opt;
    unsigned long s_sb_block;
    uid_t s_resuid;
    gid_t s_resgid;
    unsigned short s_mount_state;
    unsigned short s_pad;
    int s_addr_per_block_bits;
    int s_desc_per_block_bits;
    int s_inode_size;
    int s_first_ino;
    spinlock s_next_gen_lock;
    uint32_t s_next_generation;
    unsigned long s_dir_count;
    uint8_t *s_debts;
    /* root of the per fs reservation window tree */
    spinlock s_rsv_window_lock;
    struct reserve_window_node s_rsv_window_head;
    /*
     * s_lock protects against concurrent modifications of s_mount_state,
     * s_blocks_last, s_overhead_last and the content of superblock's
     * buffer pointed to by sbi->s_es.
     *
     * Note: It is used in show_options() to provide a consistent view
     * of the mount options.
     */
    spinlock s_lock;
};

/*
 * Define EXT2_RESERVATION to reserve data blocks for expanding files
 */
#define EXT2_DEFAULT_RESERVE_BLOCKS     8
/*max window size: 1024(direct blocks) + 3([t,d]indirect blocks) */
#define EXT2_MAX_RESERVE_BLOCKS     1027
#define EXT2_RESERVE_WINDOW_NOT_ALLOCATED 0
/*
 * The second extended file system version
 */
#define EXT2FS_DATE		"95/08/09"
#define EXT2FS_VERSION		"0.5b"


/*
 * Special inode numbers
 */
#define	EXT2_BAD_INO		 1	/* Bad blocks inode */
#define EXT2_ROOT_INO		 2	/* Root inode */
#define EXT2_BOOT_LOADER_INO	 5	/* Boot loader inode */
#define EXT2_UNDEL_DIR_INO	 6	/* Undelete directory inode */

/* First non-reserved inode for old ext2 filesystems */
#define EXT2_GOOD_OLD_FIRST_INO	11

/*
 * Macro-instructions used to manage several block sizes
 */
#define EXT2_MIN_BLOCK_SIZE		1024
#define	EXT2_MAX_BLOCK_SIZE		4096
#define EXT2_MIN_BLOCK_LOG_SIZE		  10

/*
 * Macro-instructions used to manage fragments
 */
#define EXT2_MIN_FRAG_SIZE		1024
#define	EXT2_MAX_FRAG_SIZE		4096
#define EXT2_MIN_FRAG_LOG_SIZE		  10

/*
 * Structure of a blocks group descriptor
 */
struct group_desc
{
    uint32_t	bg_block_bitmap;		/* Blocks bitmap block */
    uint32_t	bg_inode_bitmap;		/* Inodes bitmap block */
    uint32_t	bg_inode_table;		/* Inodes table block */
    uint16_t	bg_free_blocks_count;	/* Free blocks count */
    uint16_t	bg_free_inodes_count;	/* Free inodes count */
    uint16_t	bg_used_dirs_count;	/* Directories count */
    uint16_t	bg_pad;
    uint32_t	bg_reserved[3];
};

/*
 * Constants relative to the data blocks
 */
#define	EXT2_NDIR_BLOCKS		12
#define	EXT2_IND_BLOCK			EXT2_NDIR_BLOCKS
#define	EXT2_DIND_BLOCK			(EXT2_IND_BLOCK + 1)
#define	EXT2_TIND_BLOCK			(EXT2_DIND_BLOCK + 1)
#define	EXT2_N_BLOCKS			(EXT2_TIND_BLOCK + 1)

/*
 * Inode flags (GETFLAGS/SETFLAGS)
 */
#define	EXT2_SECRM_FL			FS_SECRM_FL	/* Secure deletion */
#define	EXT2_UNRM_FL			FS_UNRM_FL	/* Undelete */
#define	EXT2_COMPR_FL			FS_COMPR_FL	/* Compress file */
#define EXT2_SYNC_FL			FS_SYNC_FL	/* Synchronous updates */
#define EXT2_IMMUTABLE_FL		FS_IMMUTABLE_FL	/* Immutable file */
#define EXT2_APPEND_FL			FS_APPEND_FL	/* writes to file may only append */
#define EXT2_NODUMP_FL			FS_NODUMP_FL	/* do not dump file */
#define EXT2_NOATIME_FL			FS_NOATIME_FL	/* do not update atime */
/* Reserved for compression usage... */
#define EXT2_DIRTY_FL			FS_DIRTY_FL
#define EXT2_COMPRBLK_FL		FS_COMPRBLK_FL	/* One or more compressed clusters */
#define EXT2_NOCOMP_FL			FS_NOCOMP_FL	/* Don't compress */
#define EXT2_ECOMPR_FL			FS_ECOMPR_FL	/* Compression error */
/* End compression flags --- maybe not all used */
#define EXT2_BTREE_FL			FS_BTREE_FL	/* btree format dir */
#define EXT2_INDEX_FL			FS_INDEX_FL	/* hash-indexed directory */
#define EXT2_IMAGIC_FL			FS_IMAGIC_FL	/* AFS directory */
#define EXT2_JOURNAL_DATA_FL		FS_JOURNAL_DATA_FL /* Reserved for ext3 */
#define EXT2_NOTAIL_FL			FS_NOTAIL_FL	/* file tail should not be merged */
#define EXT2_DIRSYNC_FL			FS_DIRSYNC_FL	/* dirsync behaviour (directories only) */
#define EXT2_TOPDIR_FL			FS_TOPDIR_FL	/* Top of directory hierarchies*/
#define EXT2_RESERVED_FL		FS_RESERVED_FL	/* reserved for ext2 lib */

#define EXT2_FL_USER_VISIBLE		FS_FL_USER_VISIBLE	/* User visible flags */
#define EXT2_FL_USER_MODIFIABLE		FS_FL_USER_MODIFIABLE	/* User modifiable flags */

/* Flags that should be inherited by new inodes from their parent. */
#define EXT2_FL_INHERITED (EXT2_SECRM_FL | EXT2_UNRM_FL | EXT2_COMPR_FL |\
               EXT2_SYNC_FL | EXT2_NODUMP_FL |\
               EXT2_NOATIME_FL | EXT2_COMPRBLK_FL |\
               EXT2_NOCOMP_FL | EXT2_JOURNAL_DATA_FL |\
               EXT2_NOTAIL_FL | EXT2_DIRSYNC_FL)

/* Flags that are appropriate for regular files (all but dir-specific ones). */
#define EXT2_REG_FLMASK (~(EXT2_DIRSYNC_FL | EXT2_TOPDIR_FL))

/* Flags that are appropriate for non-directories/regular files. */
#define EXT2_OTHER_FLMASK (EXT2_NODUMP_FL | EXT2_NOATIME_FL)

/* Mask out flags that are inappropriate for the given type of inode. */
static inline uint32_t mask_flags(mode_t mode, uint32_t flags)
{
    if (S_ISDIR(mode))
        return flags;
    else if (S_ISREG(mode))
        return flags & EXT2_REG_FLMASK;
    else
        return flags & EXT2_OTHER_FLMASK;
}

/*
 * ioctl commands
 */
#define	EXT2_IOC_GETFLAGS		FS_IOC_GETFLAGS
#define	EXT2_IOC_SETFLAGS		FS_IOC_SETFLAGS
#define	EXT2_IOC_GETVERSION		FS_IOC_GETVERSION
#define	EXT2_IOC_SETVERSION		FS_IOC_SETVERSION
#define	EXT2_IOC_GETRSVSZ		_IOR('f', 5, long)
#define	EXT2_IOC_SETRSVSZ		_IOW('f', 6, long)

/*
 * ioctl commands in 32 bit emulation
 */
#define EXT2_IOC32_GETFLAGS		FS_IOC32_GETFLAGS
#define EXT2_IOC32_SETFLAGS		FS_IOC32_SETFLAGS
#define EXT2_IOC32_GETVERSION		FS_IOC32_GETVERSION
#define EXT2_IOC32_SETVERSION		FS_IOC32_SETVERSION

/*
 * Structure of an inode on the disk
 */
struct inode
{
    uint16_t	i_mode;		/* File mode */
    uint16_t	i_uid;		/* Low 16 bits of Owner Uid */
    uint32_t	i_size;		/* Size in bytes */
    uint32_t	i_atime;	/* Access time */
    uint32_t	i_ctime;	/* Creation time */
    uint32_t	i_mtime;	/* Modification time */
    uint32_t	i_dtime;	/* Deletion Time */
    uint16_t	i_gid;		/* Low 16 bits of Group Id */
    uint16_t	i_links_count;	/* Links count */
    uint32_t	i_blocks;	/* Blocks count */
    uint32_t	i_flags;	/* File flags */
    union {
        struct {
            uint32_t  l_i_reserved1;
        } linux1;
        struct {
            uint32_t  h_i_translator;
        } hurd1;
        struct {
            uint32_t  m_i_reserved1;
        } masix1;
    } osd1;				/* OS dependent 1 */
    uint32_t	i_block[EXT2_N_BLOCKS];/* Pointers to blocks */
    uint32_t	i_generation;	/* File version (for NFS) */
    uint32_t	i_file_acl;	/* File ACL */
    uint32_t	i_dir_acl;	/* Directory ACL */
    uint32_t	i_faddr;	/* Fragment address */
    union {
        struct {
            uint8_t	l_i_frag;	/* Fragment number */
            uint8_t	l_i_fsize;	/* Fragment size */
            uint16_t	i_pad1;
            uint16_t	l_i_uid_high;	/* these 2 fields    */
            uint16_t	l_i_gid_high;	/* were reserved2[0] */
            uint32_t	l_i_reserved2;
        } linux2;
        struct {
            uint8_t	h_i_frag;	/* Fragment number */
            uint8_t	h_i_fsize;	/* Fragment size */
            uint16_t	h_i_mode_high;
            uint16_t	h_i_uid_high;
            uint16_t	h_i_gid_high;
            uint32_t	h_i_author;
        } hurd2;
        struct {
            uint8_t	m_i_frag;	/* Fragment number */
            uint8_t	m_i_fsize;	/* Fragment size */
            uint16_t	m_pad1;
            uint32_t	m_i_reserved2[2];
        } masix2;
    } osd2;				/* OS dependent 2 */
};

#define i_size_high	i_dir_acl

#define i_reserved1	osd1.linux1.l_i_reserved1
#define i_frag		osd2.linux2.l_i_frag
#define i_fsize		osd2.linux2.l_i_fsize
#define i_uid_low	i_uid
#define i_gid_low	i_gid
#define i_uid_high	osd2.linux2.l_i_uid_high
#define i_gid_high	osd2.linux2.l_i_gid_high
#define i_reserved2	osd2.linux2.l_i_reserved2

/*
 * File system states
 */
#define	EXT2_VALID_FS			0x0001	/* Unmounted cleanly */
#define	EXT2_ERROR_FS			0x0002	/* Errors detected */

/*
 * Mount flags
 */
#define EXT2_MOUNT_CHECK		0x000001  /* Do mount-time checks */
#define EXT2_MOUNT_OLDALLOC		0x000002  /* Don't use the new Orlov allocator */
#define EXT2_MOUNT_GRPID		0x000004  /* Create files with directory's group */
#define EXT2_MOUNT_DEBUG		0x000008  /* Some debugging messages */
#define EXT2_MOUNT_ERRORS_CONT		0x000010  /* Continue on errors */
#define EXT2_MOUNT_ERRORS_RO		0x000020  /* Remount fs ro on errors */
#define EXT2_MOUNT_ERRORS_PANIC		0x000040  /* Panic on errors */
#define EXT2_MOUNT_MINIX_DF		0x000080  /* Mimics the Minix statfs */
#define EXT2_MOUNT_NOBH			0x000100  /* No buffer_heads */
#define EXT2_MOUNT_NO_UID32		0x000200  /* Disable 32-bit UIDs */
#define EXT2_MOUNT_XATTR_USER		0x004000  /* Extended user attributes */
#define EXT2_MOUNT_POSIX_ACL		0x008000  /* POSIX Access Control Lists */
#define EXT2_MOUNT_XIP			0x010000  /* Obsolete, use DAX */
#define EXT2_MOUNT_USRQUOTA		0x020000  /* user quota */
#define EXT2_MOUNT_GRPQUOTA		0x040000  /* group quota */
#define EXT2_MOUNT_RESERVATION		0x080000  /* Preallocation */
#ifdef CONFIG_FS_DAX
#define EXT2_MOUNT_DAX			0x100000  /* Direct Access */
#else
#define EXT2_MOUNT_DAX			0
#endif


#define clear_opt(o, opt)		o &= ~EXT2_MOUNT_##opt
#define set_opt(o, opt)			o |= EXT2_MOUNT_##opt
#define test_opt(sb, opt)		(EXT2_SB(sb)->s_mount_opt & \
                     EXT2_MOUNT_##opt)
/*
 * Maximal mount counts between two filesystem checks
 */
#define EXT2_DFL_MAX_MNT_COUNT		20	/* Allow 20 mounts */
#define EXT2_DFL_CHECKINTERVAL		0	/* Don't use interval check */

/*
 * Behaviour when detecting errors
 */
#define EXT2_ERRORS_CONTINUE		1	/* Continue execution */
#define EXT2_ERRORS_RO			2	/* Remount fs read-only */
#define EXT2_ERRORS_PANIC		3	/* Panic */
#define EXT2_ERRORS_DEFAULT		EXT2_ERRORS_CONTINUE

constexpr uint16_t EXT2_SUPER_MAGIC = 0xEF53;

/*
 * Structure of the super block
 */
struct superblock {
    uint32_t	s_inodes_count;		/* Inodes count */
    uint32_t	s_blocks_count;		/* Blocks count */
    uint32_t	s_r_blocks_count;	/* Reserved blocks count */
    uint32_t	s_free_blocks_count;	/* Free blocks count */
    uint32_t	s_free_inodes_count;	/* Free inodes count */
    uint32_t	s_first_data_block;	/* First Data Block */
    uint32_t	s_log_block_size;	/* Block size */
    uint32_t	s_log_frag_size;	/* Fragment size */
    uint32_t	s_blocks_per_group;	/* # Blocks per group */
    uint32_t	s_frags_per_group;	/* # Fragments per group */
    uint32_t	s_inodes_per_group;	/* # Inodes per group */
    uint32_t	s_mtime;		/* Mount time */
    uint32_t	s_wtime;		/* Write time */
    uint16_t	s_mnt_count;		/* Mount count */
    uint16_t	s_max_mnt_count;	/* Maximal mount count */
    uint16_t	s_magic;		/* Magic signature */
    uint16_t	s_state;		/* File system state */
    uint16_t	s_errors;		/* Behaviour when detecting errors */
    uint16_t	s_minor_rev_level; 	/* minor revision level */
    uint32_t	s_lastcheck;		/* time of last check */
    uint32_t	s_checkinterval;	/* max. time between checks */
    uint32_t	s_creator_os;		/* OS */
    uint32_t	s_rev_level;		/* Revision level */
    uint16_t	s_def_resuid;		/* Default uid for reserved blocks */
    uint16_t	s_def_resgid;		/* Default gid for reserved blocks */
    /*
     * These fields are for EXT2_DYNAMIC_REV superblocks only.
     *
     * Note: the difference between the compatible feature set and
     * the incompatible feature set is that if there is a bit set
     * in the incompatible feature set that the kernel doesn't
     * know about, it should refuse to mount the filesystem.
     *
     * e2fsck's requirements are more strict; if it doesn't know
     * about a feature in either the compatible or incompatible
     * feature set, it must abort and not try to meddle with
     * things it doesn't understand...
     */
    uint32_t	s_first_ino; 		/* First non-reserved inode */
    uint16_t   s_inode_size; 		/* size of inode structure */
    uint16_t	s_block_group_nr; 	/* block group # of this superblock */
    uint32_t	s_feature_compat; 	/* compatible feature set */
    uint32_t	s_feature_incompat; 	/* incompatible feature set */
    uint32_t	s_feature_ro_compat; 	/* readonly-compatible feature set */
    uint8_t	s_uuid[16];		/* 128-bit uuid for volume */
    char	s_volume_name[16]; 	/* volume name */
    char	s_last_mounted[64]; 	/* directory where last mounted */
    uint32_t	s_algorithm_usage_bitmap; /* For compression */
    /*
     * Performance hints.  Directory preallocation should only
     * happen if the EXT2_COMPAT_PREALLOC flag is on.
     */
    uint8_t	s_prealloc_blocks;	/* Nr of blocks to try to preallocate*/
    uint8_t	s_prealloc_dir_blocks;	/* Nr to preallocate for dirs */
    uint16_t	s_padding1;
    /*
     * Journaling support valid if EXT3_FEATURE_COMPAT_HAS_JOURNAL set.
     */
    uint8_t	s_journal_uuid[16];	/* uuid of journal superblock */
    uint32_t	s_journal_inum;		/* inode number of journal file */
    uint32_t	s_journal_dev;		/* device number of journal file */
    uint32_t	s_last_orphan;		/* start of list of inodes to delete */
    uint32_t	s_hash_seed[4];		/* HTREE hash seed */
    uint8_t	s_def_hash_version;	/* Default hash version to use */
    uint8_t	s_reserved_char_pad;
    uint16_t	s_reserved_word_pad;
    uint32_t	s_default_mount_opts;
    uint32_t	s_first_meta_bg; 	/* First metablock block group */
    uint32_t	s_reserved[190];	/* Padding to the end of the block */
};

/*
 * Codes for operating systems
 */
#define EXT2_OS_LINUX		0
#define EXT2_OS_HURD		1
#define EXT2_OS_MASIX		2
#define EXT2_OS_FREEBSD		3
#define EXT2_OS_LITES		4

/*
 * Revision levels
 */
#define EXT2_GOOD_OLD_REV	0	/* The good old (original) format */
#define EXT2_DYNAMIC_REV	1 	/* V2 format w/ dynamic inode sizes */

#define EXT2_CURRENT_REV	EXT2_GOOD_OLD_REV
#define EXT2_MAX_SUPP_REV	EXT2_DYNAMIC_REV

#define EXT2_GOOD_OLD_INODE_SIZE 128

#define EXT2_FEATURE_COMPAT_DIR_PREALLOC	0x0001
#define EXT2_FEATURE_COMPAT_IMAGIC_INODES	0x0002
#define EXT3_FEATURE_COMPAT_HAS_JOURNAL		0x0004
#define EXT2_FEATURE_COMPAT_EXT_ATTR		0x0008
#define EXT2_FEATURE_COMPAT_RESIZE_INO		0x0010
#define EXT2_FEATURE_COMPAT_DIR_INDEX		0x0020
#define EXT2_FEATURE_COMPAT_ANY			0xffffffff

#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER	0x0001
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE	0x0002
#define EXT2_FEATURE_RO_COMPAT_BTREE_DIR	0x0004
#define EXT2_FEATURE_RO_COMPAT_ANY		0xffffffff

#define EXT2_FEATURE_INCOMPAT_COMPRESSION	0x0001
#define EXT2_FEATURE_INCOMPAT_FILETYPE		0x0002
#define EXT3_FEATURE_INCOMPAT_RECOVER		0x0004
#define EXT3_FEATURE_INCOMPAT_JOURNAL_DEV	0x0008
#define EXT2_FEATURE_INCOMPAT_META_BG		0x0010
#define EXT2_FEATURE_INCOMPAT_ANY		0xffffffff

#define EXT2_FEATURE_COMPAT_SUPP	EXT2_FEATURE_COMPAT_EXT_ATTR
#define EXT2_FEATURE_INCOMPAT_SUPP	(EXT2_FEATURE_INCOMPAT_FILETYPE| \
                     EXT2_FEATURE_INCOMPAT_META_BG)
#define EXT2_FEATURE_RO_COMPAT_SUPP	(EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER| \
                     EXT2_FEATURE_RO_COMPAT_LARGE_FILE| \
                     EXT2_FEATURE_RO_COMPAT_BTREE_DIR)
#define EXT2_FEATURE_RO_COMPAT_UNSUPPORTED	~EXT2_FEATURE_RO_COMPAT_SUPP
#define EXT2_FEATURE_INCOMPAT_UNSUPPORTED	~EXT2_FEATURE_INCOMPAT_SUPP

/*
 * Default values for user and/or group using reserved blocks
 */
#define	EXT2_DEF_RESUID		0
#define	EXT2_DEF_RESGID		0

/*
 * Default mount options
 */
#define EXT2_DEFM_DEBUG		0x0001
#define EXT2_DEFM_BSDGROUPS	0x0002
#define EXT2_DEFM_XATTR_USER	0x0004
#define EXT2_DEFM_ACL		0x0008
#define EXT2_DEFM_UID16		0x0010
    /* Not used by ext2, but reserved for use by ext3 */
#define EXT3_DEFM_JMODE		0x0060
#define EXT3_DEFM_JMODE_DATA	0x0020
#define EXT3_DEFM_JMODE_ORDERED	0x0040
#define EXT3_DEFM_JMODE_WBACK	0x0060

/*
 * The new version of the directory entry.  Since EXT2 structures are
 * stored in intel byte order, and the name_len field could never be
 * bigger than 255 chars, it's safe to reclaim the extra byte for the
 * file_type field.
 */
struct dir_entry {
    uint32_t	inode;			/* Inode number */
    uint16_t	rec_len;		/* Directory entry length */
    uint8_t	name_len;		/* Name length */
    uint8_t	file_type;
    char	name[];			/* File name, up to EXT2_NAME_LEN */
};

/*
 * Ext2 directory file types.  Only the low 3 bits are used.  The
 * other bits are reserved for now.
 */
enum {
    EXT2_FT_UNKNOWN		= 0,
    EXT2_FT_REG_FILE	= 1,
    EXT2_FT_DIR		= 2,
    EXT2_FT_CHRDEV		= 3,
    EXT2_FT_BLKDEV		= 4,
    EXT2_FT_FIFO		= 5,
    EXT2_FT_SOCK		= 6,
    EXT2_FT_SYMLINK		= 7,
    EXT2_FT_MAX
};

/*
 * EXT2_DIR_PAD defines the directory entries boundaries
 *
 * NOTE: It must be a multiple of 4
 */
#define EXT2_DIR_PAD		 	4
#define EXT2_DIR_ROUND 			(EXT2_DIR_PAD - 1)
#define EXT2_DIR_REC_LEN(name_len)	(((name_len) + 8 + EXT2_DIR_ROUND) & \
                     ~EXT2_DIR_ROUND)
#define EXT2_MAX_REC_LEN		((1<<16)-1)

/*
 * ext2 mount options
 */
struct mount_options {
    unsigned long s_mount_opt;
    uid_t s_resuid;
    gid_t s_resgid;
};

/*
 * second extended file system inode data in memory
 */
struct inode_info : fs::inode {
    uint32_t	i_data[15];
    uint32_t	i_flags;
    uint32_t	i_faddr;
    uint8_t	    i_frag_no;
    uint8_t	    i_frag_size;
    uint16_t	i_state;
    uint32_t	i_file_acl;
    uint32_t	i_dir_acl;
    uint32_t	i_dtime;

    /*
     * i_block_group is the number of the block group which contains
     * this file's inode.  Constant across the lifetime of the inode,
     * it is used for making block allocation decisions - we try to
     * place a file's data blocks near its inode block, and new inodes
     * near to their parent directory's inode.
     */
    uint32_t	i_block_group;

    /* block reservation info */
    struct block_alloc_info *i_block_alloc_info;

    uint32_t	i_dir_start_lookup;
#ifdef CONFIG_EXT2_FS_XATTR
    /*
     * Extended attributes can be read independently of the main file
     * data. Taking i_mutex even when reading would cause contention
     * between readers of EAs and writers of regular file data, so
     * instead we synchronize on xattr_sem when reading or changing
     * EAs.
     */
    struct rw_semaphore xattr_sem;
#endif
    // FIXME add rwlock
    // rwlock_t i_meta_lock;

    /*
     * truncate_mutex is for serialising truncate() against
     * getblock().  It also protects the internals of the inode's
     * reservation data structures: reserve_window and
     * reserve_window_node.
     */
    mutex truncate_mutex;
    linked_list<ino_t> i_orphan;	/* unlinked but open inodes */
#ifdef CONFIG_QUOTA
    struct dquot *i_dquot[MAXQUOTAS];
#endif
};

/*
 * Inode dynamic state flags
 */
#define EXT2_STATE_NEW			0x00000001 /* inode is newly created */

}
}

#endif  /* _EXT2_H_ */
