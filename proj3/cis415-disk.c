/**********************************************************************

   File          : cis415-disk.c

   Description   : File system function implementations
                   (see .h for applications)

   Last Modified : Thu May 24 19:07:00 PDT 2012
   By            : Kevin Butler, Trent Jaeger

***********************************************************************/
/**********************************************************************
Copyright (c) 2011 The University of Oregon, 2008 The Pennsylvania State University
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
    * Neither the name of The University of Oregon, The Pennsylvania State University nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***********************************************************************/

/* Include Files */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Project Include Files */
#include "cis415-filesys.h"
#include "cis415-disk.h"
#include "cis415-list.h"
#include "cis415-util.h"

/* Definitions */

/* program variables */

/* Functions */

/**********************************************************************

    Function    : diskDirInitialize
    Description : Initialize the root directory on disk
    Inputs      : directory reference
    Outputs     : 0 if success, -1 on error

***********************************************************************/

int diskDirInitialize( ddir_t *ddir )
{
  /* Local variables */
  dblock_t *first_dentry_block;
  int i;
  ddh_t *ddh; 

  /* clear disk directory object */
  memset( ddir, 0, FS_BLOCKSIZE );

  /* initialize disk directory fields */
  ddir->buckets = ( FS_BLOCKSIZE - sizeof(ddir_t) ) / (sizeof(ddh_t));
  ddir->freeblk = 2;           /* first free block in system is block 2, see below */
  ddir->free = 0;              /* dentry offset in that block */

  /* assign block 2 (third block) as the first dentry block */
  first_dentry_block = (dblock_t *)disk2addr( fs->base, ( 2 * FS_BLOCKSIZE ));
  memset( first_dentry_block, 0, FS_BLOCKSIZE );
  first_dentry_block->free = DENTRY_BLOCK;
  first_dentry_block->st.dentry_map = DENTRY_MAP;
  first_dentry_block->next = BLK_INVALID;

  /* initialize ddir hash table */
  ddh = (ddh_t *)ddir->data;     /* start of hash table data -- in ddh_t's */
  for ( i = 0; i < ddir->buckets; i++ ) {
    (ddh+i)->next_dentry = BLK_SHORT_INVALID;
    (ddh+i)->next_slot = BLK_SHORT_INVALID;
  }

  return 0;  
}


/**********************************************************************

    Function    : diskReadDir
    Description : Retrieve the on-disk directory -- only one in this case
    Inputs      : name - name of the file 
    Outputs     : on-disk directory or -1 if error

***********************************************************************/

ddir_t *diskReadDir( char *name ) 
{
  return ((ddir_t *)disk2addr( fs->base, dfs->root ));
}


/**********************************************************************

    Function    : diskFindDentry
    Description : Retrieve the on-disk dentry from the disk directory
    Inputs      : diskdir - on-disk directory
                  name - name of the file 
    Outputs     : on-disk dentry or NULL if error

***********************************************************************/

ddentry_t *diskFindDentry( ddir_t *diskdir, char *name ) 
{
  int key = fsMakeKey( name, diskdir->buckets );
  ddh_t *ddh = (ddh_t *)&diskdir->data[key];

  while (( ddh->next_dentry != BLK_SHORT_INVALID ) || ( ddh->next_slot != BLK_SHORT_INVALID )) {
    dblock_t *dblk = (dblock_t *)disk2addr( fs->base, (block2offset( ddh->next_dentry )));
    ddentry_t *disk_dentry = (ddentry_t *)disk2addr( dblk, dentry2offset( ddh->next_slot ));
    
    if ( strcmp( disk_dentry->name, name ) == 0 ) {
      return disk_dentry;
    }

    ddh = &disk_dentry->next;
  }

  return (ddentry_t *)NULL;  
}


/**********************************************************************

    Function    : diskFindFile
    Description : Retrieve the on-disk file from the on-disk dentry
    Inputs      : disk_dentry - on-disk dentry
    Outputs     : on-disk file control block or NULL if error

***********************************************************************/

fcb_t *diskFindFile( ddentry_t *disk_dentry ) 
{
  if ( disk_dentry->block != BLK_INVALID ) {
    dblock_t *blk =  (dblock_t *)disk2addr( fs->base, (block2offset( disk_dentry->block )));
    return (fcb_t *)disk2addr( blk, sizeof(dblock_t) );
  }

  errorMessage("diskFindFile: no such file");
  printf("\nfile name = %s\n", disk_dentry->name);
  return (fcb_t *)NULL;  
}


/**********************************************************************

    Function    : diskCreateDentry
    Description : Create disk entry for the dentry on directory
    Inputs      : base - ptr to base of file system on disk
                  dir - in-memory directory
                  dentry - in-memory dentry
    Outputs     : none

***********************************************************************/

void diskCreateDentry( unsigned int base, dir_t *dir, dentry_t *dentry ) 
{
  ddir_t *diskdir = dir->diskdir;
  ddentry_t *disk_dentry;
  dblock_t *dblk, *nextblk;
  ddh_t *ddh;
  int empty = 0;
  int key;

  /* find location for new on-disk dentry */
  dblk = (dblock_t *)disk2addr( base, (block2offset( diskdir->freeblk )));
  disk_dentry = (ddentry_t *)disk2addr( dblk, dentry2offset( diskdir->free ));

  /* update disk dentry with dentry's data */
  strcpy( disk_dentry->name, dentry->name );
  disk_dentry->block = BLK_INVALID;

  /* push disk dentry into on-disk hashtable */
  key = fsMakeKey( disk_dentry->name, diskdir->buckets );
  ddh = diskDirBucket( diskdir, key );
  /* at diskdir's hashtable bucket "key", make this disk_dentry the next head
     and link to the previous head */
  disk_dentry->next.next_dentry = ddh->next_dentry;
  disk_dentry->next.next_slot = ddh->next_slot;
  ddh->next_dentry = diskdir->freeblk;
  ddh->next_slot = diskdir->free;

  /* associate dentry with ddentry */
  dentry->diskdentry = disk_dentry;
  
  /* set this disk_dentry as no longer free in the block */
  clearbit( dblk->st.dentry_map, diskdir->free, DENTRY_MAX );

  /* update free reference for dir */
  /* first the block, if all dentry space has been consumed */
  if ( dblk->st.dentry_map == 0 ) { /* no more space for dentries here */
    /* need another directory block for disk dentries */
    if ( dblk->next == BLK_INVALID ) {       /* get next file system free block */
      diskdir->freeblk = dfs->firstfree;
      
      /* update file system's free blocks */
      nextblk = (dblock_t *)disk2addr( base, block2offset( dfs->firstfree ));
      dfs->firstfree = nextblk->next;
      nextblk->free = DENTRY_BLOCK;   /* this is now a dentry block */
      nextblk->st.dentry_map = DENTRY_MAP;
      nextblk->next = BLK_INVALID;
    }
    else { /* otherwise use the next dentry block */
      diskdir->freeblk = dblk->next;      
    }
  }

  /* now update the free entry slot in the block */
  /* find the empty dentry slot */
  empty = findbit( dblk->st.dentry_map, DENTRY_MAX );
  diskdir->free = empty;

  if (empty == BLK_INVALID ) {
    errorMessage("diskCreateDentry: bad bitmap");
    return;
  }      
}


/**********************************************************************

    Function    : diskCreateFile
    Description : Create file block for the new file
    Inputs      : base - ptr to base of file system on disk
                  dentry - in-memory dentry
                  file - in-memory file
    Outputs     : none

***********************************************************************/

void diskCreateFile( unsigned int base, dentry_t *dentry, file_t *file )
{
  dblock_t *fblk;
  fcb_t *fcb;
  ddentry_t *disk_dentry;
  int i;

  /* find a file block in file system */
  fblk = (dblock_t *)disk2addr( base, (block2offset( dfs->firstfree )));
  fcb = (fcb_t *)disk2addr( fblk, sizeof( dblock_t ));   /* file is offset from block info */

  /* set file data into file block */
  fcb->flags = file->flags;

  /* initial on-disk block information for file */  
  for ( i = 0; i < FILE_BLOCKS; i++ ) {
    fcb->blocks[i] = BLK_INVALID;   /* initialize to empty */
  }

  /* associate file with the on-disk file */
  file->diskfile = fcb;

  /* get on-disk dentry */
  disk_dentry = dentry->diskdentry;

  /* set file block in on-disk dentry */
  disk_dentry->block = dfs->firstfree;

  /* update next freeblock in file system */
  dfs->firstfree = fblk->next;

  /* mark block as a file block */
  fblk->free = FILE_BLOCK;
}


/**********************************************************************

    Function    : diskWrite
    Description : Write the buffer to the disk
    Inputs      : fcb - file control block
                  block - index to block to be written
                  buf - data to be written
                  bytes - the number of bytes to write
                  offset - offset from start of file
                  sofar - bytes written so far
    Outputs     : number of bytes written or -1 on error 

***********************************************************************/

unsigned int diskWrite( fcb_t *fcb, unsigned int block, 
			char *buf, unsigned int bytes, 
			unsigned int offset, unsigned int sofar ) {
	
	dblock_t *dblk;
	char *start, *end, *data;			// declear pointers
	int block_bytes;
	unsigned int blk_offset = offset % FS_BLKDATA;
	
	dblk = (dblock_t *)disk2addr(fs->base, (block2offset(block)));
	data = (char *)disk2addr(dblk, sizeof(dblock_t));
	start = (char *)disk2addr(data, blk_offset);
	end = (char *)disk2addr(fs->base, (block2offset((block+1))));
	block_bytes = min((end-start), (bytes - sofar));

	memcpy(start, buf, block_bytes);

  return block_bytes;  
}


/**********************************************************************

    Function    : diskRead
    Description : read the buffer from the disk
    Inputs      : fcb - file control block
                  block - index to file block to read
                  buf - buffer for data
                  bytes - the number of bytes to read
                  offset - offset from start of file
                  sofar - bytes read so far 
    Outputs     : number of bytes read or -1 on error 

***********************************************************************/

unsigned int diskRead( fcb_t *fcb, unsigned int block, 
		       char *buf, unsigned int bytes, 
		       unsigned int offset, unsigned int sofar )
{
  dblock_t *dblk;
  char *start, *end, *data;
  int block_bytes;
  unsigned int blk_offset = offset % FS_BLKDATA;

  /* compute the block addresses and range */
  dblk = (dblock_t *)disk2addr( fs->base, (block2offset( block )));
  data = (char *)disk2addr( dblk, sizeof(dblock_t) );
  start = (char *)disk2addr( data, blk_offset );
  end = (char *)disk2addr( fs->base, (block2offset( (block+1) )));
  block_bytes = min(( end - start ), ( bytes - sofar ));

  /* do the write */
  memcpy( buf, start, block_bytes );

  return block_bytes;  
}


/**********************************************************************

    Function    : diskGetBlock
    Description : Get the block corresponding to this file location
    Inputs      : file - in-memory file pointer
                  index - block index in file
    Outputs     : block index or BLK_INVALID

***********************************************************************/

unsigned int diskGetBlock( file_t *file, unsigned int index )
{
  fcb_t *fcb = file->diskfile;
  int dblk_index;
  dblock_t *dblk;

  if ( fcb == NULL ) {
    errorMessage("diskGetBlock: No file control block for file");
    return BLK_INVALID;
  }

  /* if the index is already in the file control block, then return that */
  dblk_index = fcb->blocks[index]; 
 
  if ( dblk_index != BLK_INVALID ) {
    return dblk_index;
  }

  /* if there is no free flock, just return */
  if ( dfs->firstfree == BLK_INVALID ) {
    return BLK_INVALID;
  }

  /* if there is no block for this location, then need to allocate */
  dblk_index = dfs->firstfree;
  
  /* update the fcb with the new block */
  fcb->blocks[index] = dblk_index;

  /* update the filesystem's next free block -- consider function */
  dblk = (dblock_t *)disk2addr( fs->base, (block2offset( dblk_index )));

  /* mark block as a file block */
  dblk->free = FILE_DATA;

  /* update next freeblock in file system */
  dfs->firstfree = dblk->next;
    
  return dblk_index;
}
