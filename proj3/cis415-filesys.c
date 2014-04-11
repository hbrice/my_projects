/**********************************************************************

   File          : cis415-filesys.c

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
#include <errno.h>
#include <sys/types.h>
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

/* macros */

/* program variables */

/* Functions */

/**********************************************************************

    Function    : fsReadDir
    Description : Return directory entry for name
    Inputs      : name - name of the file 
    Outputs     : new in-memory directory or NULL

***********************************************************************/

dir_t *fsReadDir( char *name )
{
  dir_t *dir;
  ddir_t *diskdir;

  /* there is only one directory in this file system */
  if ( fs->dir != NULL ) {
    return fs->dir;
  }

  /* otherwise retrieve the directory from the disk */
  diskdir = diskReadDir( name );

  /* build an in-memory directory for the returned dir */
  dir = (dir_t *)malloc(sizeof(dir_t));

  if ( dir == NULL ) {
    errorMessage("fsReadDir: could not alloc dir");
    return (dir_t *)NULL;
  }

  /* store diskdir with dir and create in-memory dir */
  dir->diskdir = diskdir;
  dir->bucket_size = diskdir->buckets;
  dir->buckets = (dentry_t **)malloc(diskdir->buckets * sizeof(dentry_t *));

  /* assign the root directory to the in-memory file system */
  fs->dir = dir;

  return( dir );
}


/**********************************************************************

    Function    : fsFindDentry
    Description : Retrieve the dentry for this file name
    Inputs      : dir - directory in which to look for name 
                  name - name of the file 
    Outputs     : new dentry or NULL if error

***********************************************************************/

dentry_t *fsFindDentry( dir_t *dir, char *name )
{
  int key = fsMakeKey( name, dir->bucket_size );
  dentry_t *dentry = inList( dir->buckets[key], name );

  /* if not cached already, have to get it from the disk */
  if ( dentry == NULL ) {
    ddentry_t *disk_dentry = diskFindDentry( dir->diskdir, name );

    if ( disk_dentry == NULL ) {
      return (dentry_t *)NULL;
    }

    /* build a blank in-memory directory entry for the new file */
    dentry = fsDentryInitialize( name, disk_dentry );    
  }

  return dentry;  
}


/**********************************************************************

    Function    : fsFindFile
    Description : Retrieve the in-memory file for this file name
    Inputs      : dentry - dentry in which to look for name 
                  name - name of the file 
                  flags - flags requested for this file
    Outputs     : new file or NULL if error

***********************************************************************/

file_t *fsFindFile( dentry_t *dentry, char *name, unsigned int flags )
{
  file_t *file = dentry->file;

  /* if not cached already, have to get it from the disk */
  if ( file == NULL ) {
    ddentry_t *disk_dentry = dentry->diskdentry;
    fcb_t *fcb;  /* on disk file */

    if ( disk_dentry == NULL ) {
      errorMessage("fsFindFile: no on-disk entry must create one first");
      return (file_t *)NULL;
    }

    fcb = diskFindFile( disk_dentry );

    if ( fcb == NULL ) {
      errorMessage("fsFindFile: no on-disk file --  must create one first");
      return (file_t *)NULL;
    }

    /* check that flags requests are included in file */
    if (( flags | fcb->flags ) != fcb->flags ) {
      errorMessage("fsFindFile: flags are not acceptable");
      return (file_t *)NULL;
    }      

    /* build a blank in-memory directory entry for the new file */
    file = fsFileInitialize( dentry, name, flags, fcb );    
  }

  return file;  
}


/**********************************************************************

    Function    : fsCacheFindFile
    Description : Find if file is in the system-wide file table
    Inputs      : filetable - system-wide file table
                  name - file name 
                  flags - file flags 
    Outputs     : file or NULL

***********************************************************************/

file_t *fsCacheFindFile( file_t **filetable, char *name, unsigned int flags )
{
  int i;

  for ( i = 0; i < FS_FILETABLE_SIZE; i++ ) {
    file_t *file = filetable[i];
    if ( (file != NULL ) && ( (strcmp( file->name, name )) == 0 ) && ( (flags | file->flags) == file->flags ))  {
      return file;
    }
  }
  
  return (file_t *)NULL;
}

/**********************************************************************

    Function    : fsDentryInitialize
    Description : Create a memory dentry for this file
    Inputs      : name - file name 
                  disk_dentry - on-disk dentry object (optional)
    Outputs     : new directory entry or NULL

***********************************************************************/

dentry_t *fsDentryInitialize( char *name, ddentry_t *disk_dentry )
{
  dentry_t *dentry = (dentry_t *)malloc(sizeof(dentry_t));

  if ( dentry == NULL ) {
    errorMessage("fsDentryInitialize: could not alloc dentry");
    return NULL;
  }

  dentry->file = (file_t *) NULL;
  strcpy( dentry->name, name );
  dentry->diskdentry = disk_dentry;
  dentry->next = (dentry_t *) NULL;

  return dentry;
}


/**********************************************************************

    Function    : fsFileInitialize
    Description : Create a memory file for the specified file
    Inputs      : dir - directory object
                  name - name of the file 
                  flags - flags for file access
                  fcb - file control block (on-disk) reference for file (optional)
    Outputs     : new file or NULL

***********************************************************************/

file_t *fsFileInitialize( dentry_t *dentry, char *name, unsigned int flags, fcb_t *fcb )
{
  file_t *file = (file_t *)malloc(sizeof(file_t));

  if ( file == NULL ) {
    errorMessage("fsFileInitialize: could not alloc file");
    return NULL;
  }

  /* construct file object */
  strcpy( file->name, name );
  file->flags = flags;
  file->size = ( fcb ? fcb->size : 0 );  /* fcb is only null at create time */
  file->diskfile = fcb;
  file->ct = 1;
  memset( file->blocks, BLK_INVALID, FILE_BLOCKS * sizeof(unsigned int) );

  /* associate dentry with file */
  dentry->file = file;

  return file;
}


/**********************************************************************

    Function    : fsAddDentry
    Description : Add the dentry to its directory
    Inputs      : dir - directory object
                  dentry - dentry object
    Outputs     : 0 if success, -1 if error 

***********************************************************************/

int fsAddDentry( dir_t *dir, dentry_t *dentry )
{
  int key;

  key = fsMakeKey( dentry->name, dir->bucket_size );

  if ( key >= dir->bucket_size ) {
    errorMessage("fsAddDentry: problem with the bucket count");
    return -1;
  }

  /* add to in-memory directory representation -- not same as disk list */
  addToList( &dir->buckets[key], dentry );

  return 0;
}


/**********************************************************************

    Function    : fsAddFile
    Description : Add the file to the system-wide open file cache
    Inputs      : filetable - system-wide file table 
                  file - file to be added
    Outputs     : an index, or -1 on error 

***********************************************************************/

int fsAddFile( file_t **filetable, file_t *file) 
{
  int i;

  for ( i = 0; i < FS_FILETABLE_SIZE; i++ ) {
    if ( filetable[i] == NULL ) {
      filetable[i] = file;
      return i;
    }
  }
  
  errorMessage("fsAddFile: system-wide file table is full");
  return -1;
}


/**********************************************************************

    Function    : fsAddProcFile
    Description : Add the file to the per-process open file cache
    Inputs      : proc - process
                  file - file to be added
    Outputs     : a file descriptor, or -1 on error 

***********************************************************************/

int fsAddProcFile( proc_t *proc, file_t *file) 
{
  int i;
  fstat_t *fstat;

  /* make the fstat structure for the per-process table */
  fstat = (fstat_t *)malloc(sizeof(fstat));
  fstat->file = file;
  fstat->offset = 0;

  /* add to the per-process file table */
  for ( i = 0; i < PROC_FILETABLE_SIZE; i++ ) {
    if ( proc->fstat_table[i] == NULL ) {
      proc->fstat_table[i] = fstat;
      /* return the index (file descriptor) */
      return i;
    }
  }

  errorMessage("fsAddProcFile: per-process file table is full");
  return -1;
}


/**********************************************************************

    Function    : fileCreate
    Description : Create directory entry and file object
    Inputs      : name - name of the file 
                  flags - creation options
    Outputs     : new file descriptor or -1 if error

***********************************************************************/

int fileCreate( char *name, unsigned int flags )
{
  dentry_t *dentry;
  int index, fd;
  dir_t *dir;
  file_t *file;

  /* =======  verify file does not already exist -- first in the filetable */
  file = fsCacheFindFile( fs->filetable, name, flags );

  if ( file ) {
    errorMessage("fileCreat: file already exists in file table");
    return -1;
  }

  /* Then on disk (with a corresponding dentry */
  /* get the in-memory representation of our directory (perhaps on disk) */
  dir = fsReadDir( name );

  /* retrieve in-memory dentry for this name */
  dentry = fsFindDentry( dir, name );
  
  if ( dentry ) {
    errorMessage("fileCreat: file already exists on disk");
    return -1;
  }

  /* =======  now build the file */
  /* build a blank in-memory directory entry for the new file */
  dentry = fsDentryInitialize( name, (ddentry_t *)NULL );

  /* add in-memory dentry to in-memory directory */
  fsAddDentry( dir, dentry );

  /* create file in memory */
  file = fsFileInitialize( dentry, name, flags, (fcb_t *)NULL );

  /* add dentry to disk */
  diskCreateDentry( (unsigned int)fs->base, dir, dentry );

  /* add file to disk */
  diskCreateFile( (unsigned int)fs->base, dentry, file );

  /* add file in system-wide file table */
  index = fsAddFile( fs->filetable, file );

  if ( index < 0 ) {
    return index;
  }

  /* add file to per-process file table */
  fd = fsAddProcFile( fs->proc, file );

  return fd;
}


/**********************************************************************

    Function    : fileOpen
    Description : Open directory entry of specified name
    Inputs      : name - name of the file 
                  flags - creation options
    Outputs     : new file descriptor or -1 if error

***********************************************************************/

int fileOpen( char *name, unsigned int flags )
{
  dentry_t *dentry;
  int index, fd;
  dir_t *dir;
  file_t *file;

  /* search for file in the system-wide file table */
  file = fsCacheFindFile( fs->filetable, name, flags );

  /* if null, get from disk */
  if ( file == NULL ) {

    /* get the in-memory representation of our directory (perhaps on disk) */
    dir = fsReadDir( name );

    /* retrieve in-memory dentry for this name */
    dentry = fsFindDentry( dir, name );

    if ( dentry == NULL ) {
      errorMessage("fileOpen: No such file on disk");
      return -1;
    }

    /* add in-memory dentry to in-memory directory */
    fsAddDentry( dir, dentry );

    /* retrieve in-memory file for this name */
    file = fsFindFile( dentry, name, flags );

    if ( file == NULL ) {
      return -1;
    }

    /* add file in system-wide file table */
    index = fsAddFile( fs->filetable, file );
  
    if ( index < 0 ) {
      return index;
    }
  }

  /* add file to per-process file table */
  fd = fsAddProcFile( fs->proc, file );

  return fd;
}


/**********************************************************************

    Function    : listDirectory
    Description : print the files in the root directory currently
    Inputs      : none
    Outputs     : number of bytes read

***********************************************************************/

void listDirectory( void )
{
  dir_t *dir;
  ddir_t *diskdir;
  ddh_t *ddh;
  int i;

  /* get the in-memory representation of our directory (perhaps on disk) */
  dir = fsReadDir( "/" );

  /* get on-disk directory */
  diskdir = dir->diskdir;

  /* list the names of all the files reachable from this directory */
  /* more appropriate file: disk.c */
  ddh = (ddh_t *)&diskdir->data[0];
  for ( i = 0; i < diskdir->buckets; i++ ) {
    ddh_t *thisddh = (ddh+i);
    while ( thisddh->next_dentry != BLK_SHORT_INVALID ) {
      dblock_t *dblk = (dblock_t *)disk2addr( fs->base, (block2offset( thisddh->next_dentry )));
      ddentry_t *disk_dentry = (ddentry_t *)disk2addr( dblk, dentry2offset( thisddh->next_slot ));

      printf("File[%d]: %s\n", i, disk_dentry->name );

      thisddh = &disk_dentry->next;
    }
  }
}


/**********************************************************************

    Function    : fileClose
    Description : close the file associated with the file descriptor
    Inputs      : fd - file descriptor
    Outputs     : none

***********************************************************************/

void fileClose( unsigned int fd )
{
  fstat_t *fstat;
  file_t *file;
  int i;

  /* get the file in per-process file structure */
  fstat = fs->proc->fstat_table[fd];

  if ( fstat == NULL ) {
    errorMessage("fileClose: No file corresponds to fd");
    return;
  }

  file = fstat->file;
  
  /* reduce reference count */
  file->ct--;
  
  /* if ref count is 0, then remove file from system-wide table */
  if ( file->ct == 0 ) {
    /* note: could save the index in the filetable */
    for ( i = 0; i < FS_FILETABLE_SIZE; i++ ) {
      if ( file == fs->filetable[i] ) {
	/* remove entry from the filetable */
	fs->filetable[i] = (file_t *)NULL;
	free( file );
	break;
      }
    }
  }
}


/**********************************************************************

    Function    : fileRead
    Description : Read specified number of bytes from the current file index
    Inputs      : fd - file descriptor
                  buf - buffer for placing data
                  bytes - number of bytes to read
    Outputs     : number of bytes read

***********************************************************************/

int fileRead( unsigned int fd, char *buf, unsigned int bytes ) {
    unsigned int total = 0;
	fstat_t *fstat;					// declear pointer
	file_t *file;					// declear pointer
	unsigned int offset;			// declear variables
	unsigned int index;
	unsigned int blockInd;
	char *temp;						// declear pointer
	unsigned int value = 0;
	fstat = fs->proc->fstat_table[fd];		// retrieve fd
	if(fstat == NULL){
		errorMessage("fstat is null");		// error handling
		return -1;
	}
	file = fstat->file;						// get real file
	offset = fstat->offset;					// offset = offset
	index = offset/FS_BLKDATA;				// current index
	while(total < bytes){					// while there is more to read call diskread
		blockInd = diskGetBlock(file, index);
		temp = (char *)malloc(FS_BLOCKSIZE * sizeof(char));	//construct buffer to read from disk
		value = diskRead(file->diskfile, blockInd, temp, bytes, offset, total);//diskfile is fcb
		total += value;
		strcat(buf, temp);	
	//	free(temp);			// free up the malloc mem
		offset = 0;			// move the offset forward by the number of bytes to read
		index ++;			// increase index
	}
    return total;
}


/**********************************************************************

    Function    : fileWrite
    Description : Write specified number of bytes starting at the current file index
    Inputs      : fd - file descriptor
                  buf - buffer to write
                  bytes - number of bytes to write
    Outputs     : number of bytes written

***********************************************************************/

int fileWrite( unsigned int fd, char *buf, unsigned int bytes ) {
	unsigned int total = 0;
	fstat_t *fstat;
	file_t *file;
	unsigned int offset;					// declear variables
	unsigned int index;
	unsigned int end;
	unsigned int blockInd;
	fstat = fs->proc->fstat_table[fd];		// retrieve fd
	if(fstat == NULL){
		errorMessage("fstat is null!!!");	//error checking
		return -1;
	}
	file = fstat->file;					// get real file (shortcut)
	offset = fstat->offset;				// offset gets the real offset
	end = offset + bytes;			// the whole thing we want to write
	index = offset/(FS_BLKDATA);
	while(total < bytes){	// while there is more to read call diskread
		blockInd = diskGetBlock(file, index);
		if(blockInd == -1){
			errorMessage("diskGetBlock fails!!!");
			break;
		}
		int len = diskWrite(file->diskfile, blockInd, buf,
												bytes, offset, total);
		total += len;						// update total
		buf += len;							// update buf
		offset = 0;							// reset the offset
		index++;							// increase index
	}

	file->size = end;					// update size of file
	fstat->offset += total;
    return total;
}


/**********************************************************************

    Function    : fileSeek
    Description : Adjust offset in per-process file entry
    Inputs      : fd - file descriptor
                  index - new offset 
    Outputs     : 0 on success, -1 on failure

***********************************************************************/

int fileSeek( unsigned int fd, unsigned int index )
{ 
	fstat_t *fstat;							// defining pointer
	fstat = fs->proc->fstat_table[fd];		// get the fd for fstat
	if(fstat == NULL){						// error handling
		errorMessage("fileSeek: there is no file corresponding to fd");
		return -1;
	}
	
	fstat->offset = index;			// set the offset to index
	return 0;
}



