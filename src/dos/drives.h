/*
 *  Copyright (C) 2002-2007  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* $Id: drives.h,v 1.38 2007/08/22 11:54:35 qbix79 Exp $ */

#ifndef _DRIVES_H__
#define _DRIVES_H__

#include <vector>
#include <sys/types.h>
#include "dos_system.h"
#include "shell.h" /* for DOS_Shell */
#include "bios.h"  /* for fatDrive */

bool WildFileCmp(const char * file, const char * wild);

class DriveManager {
public:
	static void AppendDisk(int drive, DOS_Drive* disk);
	static void InitializeDrive(int drive);
	static int UnmountDrive(int drive);
//	static void CycleDrive(bool pressed);
//	static void CycleDisk(bool pressed);
	static void CycleAllDisks(void);
	static void Init(Section* sec);
	
private:
	static struct DriveInfo {
		std::vector<DOS_Drive*> disks;
		Bit32u currentDisk;
	} driveInfos[DOS_DRIVES];
	
	static int currentDrive;
};

class localDrive : public DOS_Drive {
public:
	localDrive(const char * startdir,Bit16u _bytes_sector,Bit8u _sectors_cluster,Bit16u _total_clusters,Bit16u _free_clusters,Bit8u _mediaid);
	virtual bool FileOpen(DOS_File * * file,char * name,Bit32u flags);
	virtual FILE *GetSystemFilePtr(char const * const name, char const * const type);
	virtual bool GetSystemFilename(char* sysName, char const * const dosName);
	virtual bool FileCreate(DOS_File * * file,char * name,Bit16u attributes);
	virtual bool FileUnlink(char * name);
	virtual bool RemoveDir(char * dir);
	virtual bool MakeDir(char * dir);
	virtual bool TestDir(char * dir);
	virtual bool FindFirst(char * _dir,DOS_DTA & dta,bool fcb_findfirst=false);
	virtual bool FindNext(DOS_DTA & dta);
	virtual bool GetFileAttr(char * name,Bit16u * attr);
	virtual bool Rename(char * oldname,char * newname);
	virtual bool AllocationInfo(Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters,Bit16u * _free_clusters);
	virtual bool FileExists(const char* name);
	virtual bool FileStat(const char* name, FileStat_Block * const stat_block);
	virtual Bit8u GetMediaByte(void);
	virtual bool isRemote(void);
	virtual bool isRemovable(void);
	virtual Bits UnMount(void);
private:
	char basedir[CROSS_LEN];
	friend void DOS_Shell::CMD_SUBST(char* args); 	
	struct {
		char srch_dir[CROSS_LEN];
	} srchInfo[MAX_OPENDIRS];

	struct {
		Bit16u bytes_sector;
		Bit8u sectors_cluster;
		Bit16u total_clusters;
		Bit16u free_clusters;
		Bit8u mediaid;
	} allocation;
};

#ifdef _MSC_VER
#pragma pack (1)
#endif
struct bootstrap {
	Bit8u  nearjmp[3] GCC_ATTRIBUTE(packed);
	Bit8u  oemname[8] GCC_ATTRIBUTE(packed);
	Bit16u bytespersector GCC_ATTRIBUTE(packed);
	Bit8u  sectorspercluster GCC_ATTRIBUTE(packed);
	Bit16u reservedsectors GCC_ATTRIBUTE(packed);
	Bit8u  fatcopies GCC_ATTRIBUTE(packed);
	Bit16u rootdirentries GCC_ATTRIBUTE(packed);
	Bit16u totalsectorcount GCC_ATTRIBUTE(packed);
	Bit8u  mediadescriptor GCC_ATTRIBUTE(packed);
	Bit16u sectorsperfat GCC_ATTRIBUTE(packed);
	Bit16u sectorspertrack GCC_ATTRIBUTE(packed);
	Bit16u headcount GCC_ATTRIBUTE(packed);
	/* 32-bit FAT extensions */
	Bit32u hiddensectorcount GCC_ATTRIBUTE(packed);
	Bit32u totalsecdword GCC_ATTRIBUTE(packed);
	Bit8u  bootcode[474] GCC_ATTRIBUTE(packed);
	Bit8u  magic1 GCC_ATTRIBUTE(packed); /* 0x55 */
	Bit8u  magic2 GCC_ATTRIBUTE(packed); /* 0xaa */
} GCC_ATTRIBUTE(packed);

struct direntry {
	Bit8u entryname[11] GCC_ATTRIBUTE(packed);
	Bit8u attrib GCC_ATTRIBUTE(packed);
	Bit8u NTRes GCC_ATTRIBUTE(packed);
	Bit8u milliSecondStamp GCC_ATTRIBUTE(packed);
	Bit16u crtTime GCC_ATTRIBUTE(packed);
	Bit16u crtDate GCC_ATTRIBUTE(packed);
	Bit16u accessDate GCC_ATTRIBUTE(packed);
	Bit16u hiFirstClust GCC_ATTRIBUTE(packed);
	Bit16u modTime GCC_ATTRIBUTE(packed);
	Bit16u modDate GCC_ATTRIBUTE(packed);
	Bit16u loFirstClust GCC_ATTRIBUTE(packed);
	Bit32u entrysize GCC_ATTRIBUTE(packed);
} GCC_ATTRIBUTE(packed);

struct partTable {
	Bit8u booter[446] GCC_ATTRIBUTE(packed);
	struct {
		Bit8u bootflag GCC_ATTRIBUTE(packed);
		Bit8u beginchs[3] GCC_ATTRIBUTE(packed);
		Bit8u parttype GCC_ATTRIBUTE(packed);
		Bit8u endchs[3] GCC_ATTRIBUTE(packed);
		Bit32u absSectStart GCC_ATTRIBUTE(packed);
		Bit32u partSize GCC_ATTRIBUTE(packed);
	} pentry[4] GCC_ATTRIBUTE(packed);
	Bit8u  magic1 GCC_ATTRIBUTE(packed); /* 0x55 */
	Bit8u  magic2 GCC_ATTRIBUTE(packed); /* 0xaa */
} GCC_ATTRIBUTE(packed);

#ifdef _MSC_VER
#pragma pack ()
#endif

class fatDrive : public DOS_Drive {
public:
	fatDrive(const char * sysFilename, Bit32u bytesector, Bit32u cylsector, Bit32u headscyl, Bit32u cylinders, Bit32u startSector);
	virtual bool FileOpen(DOS_File * * file,char * name,Bit32u flags);
	virtual bool FileCreate(DOS_File * * file,char * name,Bit16u attributes);
	virtual bool FileUnlink(char * name);
	virtual bool RemoveDir(char * dir);
	virtual bool MakeDir(char * dir);
	virtual bool TestDir(char * dir);
	virtual bool FindFirst(char * _dir,DOS_DTA & dta,bool fcb_findfirst=false);
	virtual bool FindNext(DOS_DTA & dta);
	virtual bool GetFileAttr(char * name,Bit16u * attr);
	virtual bool Rename(char * oldname,char * newname);
	virtual bool AllocationInfo(Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters,Bit16u * _free_clusters);
	virtual bool FileExists(const char* name);
	virtual bool FileStat(const char* name, FileStat_Block * const stat_block);
	virtual Bit8u GetMediaByte(void);
	virtual bool isRemote(void);
	virtual bool isRemovable(void);
	virtual Bits UnMount(void);
public:
	Bit32u getAbsoluteSectFromBytePos(Bit32u startClustNum, Bit32u bytePos);
	Bit32u getSectorSize(void);
	Bit32u getAbsoluteSectFromChain(Bit32u startClustNum, Bit32u logicalSector);
	bool allocateCluster(Bit32u useCluster, Bit32u prevCluster);
	Bit32u appendCluster(Bit32u startCluster);
	void deleteClustChain(Bit32u startCluster);
	Bit32u getFirstFreeClust(void);
	bool directoryBrowse(Bit32u dirClustNumber, direntry *useEntry, Bit32s entNum);
	bool directoryChange(Bit32u dirClustNumber, direntry *useEntry, Bit32s entNum);
	imageDisk *loadedDisk;
	bool created_succesfully;
private:
	Bit32u getClusterValue(Bit32u clustNum);
	void setClusterValue(Bit32u clustNum, Bit32u clustValue);
	Bit32u getClustFirstSect(Bit32u clustNum);
	bool FindNextInternal(Bit32u dirClustNumber, DOS_DTA & dta, direntry *foundEntry);
	bool getDirClustNum(char * dir, Bit32u * clustNum, bool parDir);
	bool getFileDirEntry(char const * const filename, direntry * useEntry, Bit32u * dirClust, Bit32u * subEntry);
	bool addDirectoryEntry(Bit32u dirClustNumber, direntry useEntry);
	void zeroOutCluster(Bit32u clustNumber);
	bool getEntryName(char *fullname, char *entname);
	friend void DOS_Shell::CMD_SUBST(char* args); 	
	struct {
		char srch_dir[CROSS_LEN];
	} srchInfo[MAX_OPENDIRS];

	struct {
		Bit16u bytes_sector;
		Bit8u sectors_cluster;
		Bit16u total_clusters;
		Bit16u free_clusters;
		Bit8u mediaid;
	} allocation;
	
	bootstrap bootbuffer;
	Bit8u fattype;
	Bit32u CountOfClusters;
	Bit32u partSectOff;
	Bit32u firstDataSector;
	Bit32u firstRootDirSect;

	Bit32u cwdDirCluster;
	Bit32u dirPosition; /* Position in directory search */
};


class cdromDrive : public localDrive
{
public:
	cdromDrive(const char driveLetter, const char * startdir,Bit16u _bytes_sector,Bit8u _sectors_cluster,Bit16u _total_clusters,Bit16u _free_clusters,Bit8u _mediaid, int& error);
	virtual bool FileOpen(DOS_File * * file,char * name,Bit32u flags);
	virtual bool FileCreate(DOS_File * * file,char * name,Bit16u attributes);
	virtual bool FileUnlink(char * name);
	virtual bool RemoveDir(char * dir);
	virtual bool MakeDir(char * dir);
	virtual bool Rename(char * oldname,char * newname);
	virtual bool GetFileAttr(char * name,Bit16u * attr);
	virtual bool FindFirst(char * _dir,DOS_DTA & dta,bool fcb_findfirst=false);
	virtual void SetDir(const char* path);
	virtual bool isRemote(void);
	virtual bool isRemovable(void);
	virtual Bits UnMount(void);
private:
	Bit8u subUnit;
	char driveLetter;
};

#ifdef _MSC_VER
#pragma pack (1)
#endif
struct isoPVD {
	Bit8u type GCC_ATTRIBUTE(packed);
	Bit8u standardIdent[5] GCC_ATTRIBUTE(packed);
	Bit8u version GCC_ATTRIBUTE(packed);
	Bit8u unused1 GCC_ATTRIBUTE(packed);
	Bit8u systemIdent[32] GCC_ATTRIBUTE(packed);
	Bit8u volumeIdent[32] GCC_ATTRIBUTE(packed);
	Bit8u unused2[8] GCC_ATTRIBUTE(packed);
	Bit32u volumeSpaceSizeL GCC_ATTRIBUTE(packed);
	Bit32u volumeSpaceSizeM GCC_ATTRIBUTE(packed);
	Bit8u unused3[32] GCC_ATTRIBUTE(packed);
	Bit16u volumeSetSizeL GCC_ATTRIBUTE(packed);
	Bit16u volumeSetSizeM GCC_ATTRIBUTE(packed);
	Bit16u volumeSeqNumberL GCC_ATTRIBUTE(packed);
	Bit16u volumeSeqNumberM GCC_ATTRIBUTE(packed);
	Bit16u logicBlockSizeL GCC_ATTRIBUTE(packed);
	Bit16u logicBlockSizeM GCC_ATTRIBUTE(packed);
	Bit32u pathTableSizeL GCC_ATTRIBUTE(packed);
	Bit32u pathTableSizeM GCC_ATTRIBUTE(packed);
	Bit32u locationPathTableL GCC_ATTRIBUTE(packed);
	Bit32u locationOptPathTableL GCC_ATTRIBUTE(packed);
	Bit32u locationPathTableM GCC_ATTRIBUTE(packed);
	Bit32u locationOptPathTableM GCC_ATTRIBUTE(packed);
	Bit8u rootEntry[34] GCC_ATTRIBUTE(packed);
	Bit32u unused4[1858] GCC_ATTRIBUTE(packed);
} GCC_ATTRIBUTE(packed);

struct isoDirEntry {
	Bit8u length GCC_ATTRIBUTE(packed);
	Bit8u extAttrLength GCC_ATTRIBUTE(packed);
	Bit32u extentLocationL GCC_ATTRIBUTE(packed);
	Bit32u extentLocationM GCC_ATTRIBUTE(packed);
	Bit32u dataLengthL GCC_ATTRIBUTE(packed);
	Bit32u dataLengthM GCC_ATTRIBUTE(packed);
	Bit8u dateYear GCC_ATTRIBUTE(packed);
	Bit8u dateMonth GCC_ATTRIBUTE(packed);
	Bit8u dateDay GCC_ATTRIBUTE(packed);
	Bit8u timeHour GCC_ATTRIBUTE(packed);
	Bit8u timeMin GCC_ATTRIBUTE(packed);
	Bit8u timeSec GCC_ATTRIBUTE(packed);
	Bit8u timeZone GCC_ATTRIBUTE(packed);
	Bit8u fileFlags GCC_ATTRIBUTE(packed);
	Bit8u fileUnitSize GCC_ATTRIBUTE(packed);
	Bit8u interleaveGapSize GCC_ATTRIBUTE(packed);
	Bit16u VolumeSeqNumberL GCC_ATTRIBUTE(packed);
	Bit16u VolumeSeqNumberM GCC_ATTRIBUTE(packed);
	Bit8u fileIdentLength GCC_ATTRIBUTE(packed);
	Bit8u ident[222] GCC_ATTRIBUTE(packed);
} GCC_ATTRIBUTE(packed);

#ifdef _MSC_VER
#pragma pack ()
#endif

#if defined (WORDS_BIGENDIAN)
#define EXTENT_LOCATION(de)	((de).extentLocationM)
#define DATA_LENGTH(de)		((de).dataLengthM)
#else
#define EXTENT_LOCATION(de)	((de).extentLocationL)
#define DATA_LENGTH(de)		((de).dataLengthL)
#endif

#define ISO_FRAMESIZE		2048
#define ISO_DIRECTORY		2
#define ISO_HIDDEN		1
#define ISO_MAX_FILENAME_LENGTH 37
#define ISO_MAXPATHNAME		256
#define ISO_FIRST_VD		16
#define IS_DIR(fileFlags)	(fileFlags & ISO_DIRECTORY)
#define IS_HIDDEN(fileFlags)	(fileFlags & ISO_HIDDEN)
#define ISO_MAX_HASH_TABLE_SIZE 	100

class isoDrive : public DOS_Drive {
public:
	isoDrive(char driveLetter, const char* device_name, Bit8u mediaid, int &error);
	~isoDrive();
	virtual bool FileOpen(DOS_File **file, char *name, Bit32u flags);
	virtual bool FileCreate(DOS_File **file, char *name, Bit16u attributes);
	virtual bool FileUnlink(char *name);
	virtual bool RemoveDir(char *dir);
	virtual bool MakeDir(char *dir);
	virtual bool TestDir(char *dir);
	virtual bool FindFirst(char *_dir, DOS_DTA &dta, bool fcb_findfirst);
	virtual bool FindNext(DOS_DTA &dta);
	virtual bool GetFileAttr(char *name, Bit16u *attr);
	virtual bool Rename(char * oldname,char * newname);
	virtual bool AllocationInfo(Bit16u *bytes_sector, Bit8u *sectors_cluster, Bit16u *total_clusters, Bit16u *free_clusters);
	virtual bool FileExists(const char *name);
   	virtual bool FileStat(const char *name, FileStat_Block *const stat_block);
	virtual Bit8u GetMediaByte(void);
	virtual void EmptyCache(void){}
	virtual bool isRemote(void);
	virtual bool isRemovable(void);
	virtual Bits UnMount(void);
	bool readSector(Bit8u *buffer, Bit32u sector);
	virtual char const* GetLabel(void) {return discLabel;};
	virtual void Activate(void);
private:
	int  readDirEntry(isoDirEntry *de, Bit8u *data);
	bool loadImage();
	bool lookupSingle(isoDirEntry *de, const char *name, Bit32u sectorStart, Bit32u length);
	bool lookup(isoDirEntry *de, const char *path);
	int  UpdateMscdex(char driveLetter, const char* physicalPath, Bit8u& subUnit);
	int  GetDirIterator(const isoDirEntry* de);
	bool GetNextDirEntry(const int dirIterator, isoDirEntry* de);
	void FreeDirIterator(const int dirIterator);
	bool ReadCachedSector(Bit8u** buffer, const Bit32u sector);
	
	struct DirIterator {
		bool valid;
		Bit32u currentSector;
		Bit32u endSector;
		Bit32u pos;
	} dirIterators[MAX_OPENDIRS];
	
	int nextFreeDirIterator;
	
	struct SectorHashEntry {
		bool valid;
		Bit32u sector;
		Bit8u data[ISO_FRAMESIZE];
	} sectorHashEntries[ISO_MAX_HASH_TABLE_SIZE];

	bool dataCD;
	isoDirEntry rootEntry;
	Bit8u mediaid;
	char fileName[CROSS_LEN];
	Bit8u subUnit;
	char driveLetter;
	char discLabel[32];
};

struct VFILE_Block;

class Virtual_Drive: public DOS_Drive {
public:
	Virtual_Drive();
	bool FileOpen(DOS_File * * file,char * name,Bit32u flags);
	bool FileCreate(DOS_File * * file,char * name,Bit16u attributes);
	bool FileUnlink(char * name);
	bool RemoveDir(char * dir);
	bool MakeDir(char * dir);
	bool TestDir(char * dir);
	bool FindFirst(char * _dir,DOS_DTA & dta,bool fcb_findfirst);
	bool FindNext(DOS_DTA & dta);
	bool GetFileAttr(char * name,Bit16u * attr);
	bool Rename(char * oldname,char * newname);
	bool AllocationInfo(Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters,Bit16u * _free_clusters);
	bool FileExists(const char* name);
	bool FileStat(const char* name, FileStat_Block* const stat_block);
	Bit8u GetMediaByte(void);
	void EmptyCache(void){}
	bool isRemote(void);
	virtual bool isRemovable(void);
	virtual Bits UnMount(void);
private:
	VFILE_Block * search_file;
};



#endif
