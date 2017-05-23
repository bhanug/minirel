#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include "minirel.h"
#include "pf.h"
#include "../pf/pfUtils.h"
#include "hf.h"
#include "../hf/hfUtils.h"
#include "am.h"
#include "../am/amUtils.h"


/* B tree*/
/* policy to go through the B tree using a particular value, if the value if len than a key in a node or a leaf then the previous pointer has to be taken, otherwise ( greater or equal) the value is check with the next keys in the node until it is less than one or until the last key (the last pointer of the node is taken then )*/
/* int AM_FindLeaf(int ifd,char* value, int* tab) algorithm: -check parameter: the size of tab is the height of the tree and the first element is the page num of root.
							 - go through the Btree with the policy explain above, and stock the pagenum of every scanned node in the array tab , at the end the pagenum of the leaf where the value has to be insert is returned 
							 - return error or HFE_OK, in case of duplicate key: the position of the first equal key in the leaf*/
AMitab_ele *AMitab;
size_t AMitab_length;

void AM_PrintTable(void){
	size_t i;
	printf("\n\n******** AM Table ********\n");
	printf("******* Length : %d *******\n",(int) AMitab_length);
	for(i=0; i<AMitab_length; i++){
		printf("**** %d | %s | %d racine_page | %d fanout | %d (nb_leaf) | %d (height_tree)\n", (int)i, AMitab[i].iname, AMitab[i].header.racine_page, AMitab[i].fanout, AMitab[i].header.nb_leaf, AMitab[i].header.height_tree);
	}
	printf("**************************\n\n");

}

void print_page(int fileDesc, int pagenum){
	char* pagebuf;
	AMitab_ele* pt;
	int error, offset, i;
	bool_t is_leaf;
	int num_keys;
	RECID recid;
	int ivalue;
	float fvalue;
	char* cvalue;

	pt = AMitab + fileDesc;
	error = PF_GetThisPage(pt->fd, pagenum, &pagebuf);
	if (error != PFE_OK)
		PF_ErrorHandler(error);

	cvalue = malloc(sizeof(pt->header.attrLength));

	offset = 0;
	memcpy((bool_t*) &is_leaf, (char*)pagebuf, sizeof(bool_t));
	offset += sizeof(bool_t);
	memcpy((int*) &num_keys, (char*) (pagebuf+offset), sizeof(int));
	offset += sizeof(int);

	if (is_leaf){
		offset += 2 * sizeof(int);
		printf("\n**** LEAF PAGE ****\n");
		printf("%d keys\n", num_keys);
		/*SUPPORTS ONLYS LEAVES FOR NOW */
		for(i=0; i<num_keys; i++){
			memcpy((RECID*) &recid, (char*) (pagebuf + offset), sizeof(RECID));
			offset += sizeof(RECID);

			printf("(%d,%d)", recid.pagenum, recid.recnum);
			
			switch (pt->header.attrType){
				case 'c':
					memcpy((char*) cvalue, (char*)(pagebuf + offset), pt->header.attrLength);
					printf(" : %s | ", cvalue);
					break;
				case 'i':
					memcpy((int*) &ivalue, (char*)(pagebuf + offset), pt->header.attrLength);
					printf(" : %d | ", ivalue);
					break;
				case 'f':
					memcpy((float*) &fvalue, (char*)(pagebuf + offset), pt->header.attrLength);
					printf(" : %f | ", fvalue);
					break;
				default:
					return AME_INVALIDATTRTYPE;
					break;
			}

			offset += pt->header.attrLength;

		}

	}else{
		offset += sizeof(int);
		printf("\n**** NODE PAGE ****\n");
	}


	printf("\n\n");
	/*free(cvalue); trouble when freeing : sigabrt*/
	error = PF_UnpinPage(pt->fd, pagenum, 0);
	if (error != PFE_OK)
		PF_ErrorHandler(error);	
}




/* For any type of node: given a value and the position of a couple ( pointer,key) in a node, return the pointer if it has to be taken or not by comparing key and value. Return 0 if the pointer is unknown.
 * the fanout is also given, in case of the couple contain the last key, then the last pointer of the node has to be taken
 * this function is created to avoid switch-case inside a loop
 */
bool_t AM_CheckPointer(int pos, int fanout, char* value, char attrType, int attrLength, char* pagebuf){
	/*use to get any type of node and leaf from a buffer page*/
	inode inod;
	fnode fnod;
	cnode cnod;
	/*ileaf* ilcast;
	fleaf* flcast;
	cleaf* clcast;*/
	
	switch(attrType){
		case 'i': 
			  /* fill the struct using offset and cast operations */
			  inod.num_keys= *((int*)pagebuf+sizeof(bool_t));
			  inod.couple=(icouple *)pagebuf+sizeof(bool_t)+sizeof(int);
			  inod.last_pt=*((int*)(pagebuf+sizeof(bool_t)+(inod.num_keys)*2*sizeof(int)+sizeof(int)));
			  
			  if( pos<fanout-1){ /* fanout -1 is the number of couple (pointer, key)*/
				if( strncmp((char*) &(inod.couple[pos].key), (char*) value,  sizeof(int)) <0) return inod.couple[pos].pagenum;
				return 0;
			   }
			   else if( pos=fanout-1) {
			   	if( strncmp((char*) &(inod.couple[pos].key), (char*) value,  sizeof(int)) >=0) return inod.last_pt;
				return 0;
			   }
			   else {
				printf( "DEBUG: pb with fanout or position given \n");
				return -1;
			   }
			   break;

		case 'c':  /* fill the struct using offset and cast operations */
			  cnod.num_keys= *((int*)pagebuf+sizeof(bool_t));
			  cnod.couple=(ccouple*)pagebuf+sizeof(bool_t)+sizeof(int);
			  cnod.last_pt=*((int*)(pagebuf+sizeof(bool_t)+(cnod.num_keys)*2*sizeof(int)+sizeof(int)));
			  if( pos<fanout-1){ /* fanout -1 is the number of couple (pointer, key)*/
				if( strncmp((char*) cnod.couple[pos].key, (char*) value,  attrLength) <0) return cnod.couple[pos].pagenum;
				return 0;
			   }
			   else if( pos=fanout-1) {
			   	if( strncmp((char*) cnod.couple[pos].key, (char*) value,  attrLength) >=0) return cnod.last_pt;
				return 0;
			   }
			   else {
				printf( "DEBUG: pb with fanout or position given \n");
				return -1;
			   }
			   break;

		case 'f':  /* fill the struct using offset and cast operations */
			  fnod.num_keys= *((int*)pagebuf+sizeof(bool_t));
			  fnod.couple=(fcouple*)pagebuf+sizeof(bool_t)+sizeof(int);
			  fnod.last_pt=*((int*)(pagebuf+sizeof(bool_t)+(fnod.num_keys)*2*sizeof(int)+sizeof(int)));

			  if( pos<(fanout-1)){ /* fanout -1 is the number of couple (pointer, key)*/
				if(  fnod.couple[pos].key > *((float*)value) ) return fnod.couple[pos].pagenum;
				return 0;
			   }
			   else if(( pos==fanout-1)) {
			   	if(  fnod.couple[pos].key <=*((float*)value) ) return fnod.last_pt;
				return 0;
			   }
			   else {
				printf( "DEBUG: pb with fanout or position given \n");
				return -1;
			   }
			   break;
		default:
			
			return AME_ATTRTYPE;
	}
}

	
	 
int AM_FindLeaf(int idesc, char* value, int* tab){
	int error,  pagenum;
	bool_t is_leaf;
	AMitab_ele* pt;
	int fanout;
	char* pagebuf;
	int i,j;
	
	
	/* all verifications of idesc and root number has already been done by the caller function */
	pt=AMitab+idesc;

	/*if(tab[0]<=1) return AME_WRONGROOT;
	*/
	/*for(i=0; i<(pt->header.height_tree);i++){

		error=PF_GetThisPage(pt->fd, tab[i], &pagebuf);
		if(error!=PFE_OK) PF_ErrorHandler(error);
		
		/*check if this node is a leaf */
		/*memcpy((bool_t*) &is_leaf, pagebuf, sizeof(bool_t));
		
		if( is_leaf==FALSE){
				fanout=pt->fanout;
			/* have to find the next child using comparison */
			/* fanout = n ==> n-1 key */
			/*	for(j=0; j<(pt->fanout-1);j++){
					}
		}
   

	}
	
	*/
	pagenum = 0;
	return pagenum;
					
					
						
			
}





/* 
 *
 */
void AM_Init(void){
	AMitab = malloc(sizeof(AMitab_ele) * AM_ITAB_SIZE);
	AMitab_length = 0;
	HF_Init();
}



int AM_CreateIndex(char *fileName, int indexNo, char attrType, int attrLength, bool_t isUnique){
	int error, fd, pagenum;
	AMitab_ele* pt;
	char* headerbuf;
/**********************add verif on all attribute check HF_OpenScan */
	/* fill the array of the hf file table*/ 
	if (AMitab_length >= AM_ITAB_SIZE){
		return AME_FULLTABLE;
	}
	pt = AMitab + AMitab_length;
	AMitab_length++;

	sprintf(pt->iname, "%s.%d", fileName, indexNo);

	error = PF_CreateFile(pt->iname);
	if(error != PFE_OK)
		PF_ErrorHandler(error);

	fd = PF_OpenFile(pt->iname);
	if(fd < 0 )
		return AME_PF;

	pt->fd = fd;
	pt->valid = TRUE;
	pt->dirty = TRUE;
	pt->header.racine_page = -1;
	/* Since on a internal node, there  is 3 int(is_leaf, pagenum of parent, number of key*/ 
	pt->fanout = ( (PF_PAGE_SIZE ) - 2*sizeof(int) - sizeof(bool_t)) / (sizeof(int) + pt->header.attrLength) + 1;
	pt->fanout_leaf = ( (PF_PAGE_SIZE - 3*sizeof(int) -sizeof(bool_t) ) / (attrLength + sizeof(RECID)) ) + 1; /* number of recid into a leaf page */
	pt->header.indexNo = indexNo;
	pt->header.attrType = attrType;
	pt->header.attrLength = attrLength;
	pt->header.height_tree = 0;
	pt->header.nb_leaf = 0;

	error = PF_AllocPage(fd, &pagenum, &headerbuf);
	if (error != PFE_OK)
		PF_ErrorHandler(error);

	if (pagenum != 1)
		return AME_PF;

	memcpy((char*) (headerbuf), (int*) &pt->header.indexNo, sizeof(int));
	memcpy((char*) (headerbuf + sizeof(int)), (int*) &pt->header.attrType, sizeof(char));
	memcpy((char*) (headerbuf + sizeof(int) + sizeof(char)), (int*) &pt->header.attrLength, sizeof(int));
	memcpy((char*) (headerbuf + 2*sizeof(int) + sizeof(char)), (int*) &pt->header.height_tree, sizeof(int));
	memcpy((char*) (headerbuf + 3*sizeof(int) + sizeof(char)), (int*) &pt->header.nb_leaf, sizeof(int));
	memcpy((char*) (headerbuf + 4*sizeof(int) + sizeof(char)), (int*) &pt->header.racine_page, sizeof(int));
	memcpy((char*) (headerbuf + 5*sizeof(int) + sizeof(char)), (int*) &pt->header.num_pages, sizeof(int));


	pt->valid = FALSE;
	
	error = PF_UnpinPage(pt->fd, pagenum, 1);
	if (error != PFE_OK)
		return PF_ErrorHandler(error);

	error = PF_CloseFile(pt->fd);
	if (error != PFE_OK)
		PF_ErrorHandler(error);


	AMitab_length--;
	printf("Index : %s created\n", pt->iname);
	return AME_OK;
}

int AM_DestroyIndex(char *fileName, int indexNo){
	int error;
	char* new_filename;

	new_filename = malloc(sizeof(fileName) + sizeof(int));
	sprintf(new_filename, "%s.%i", fileName, indexNo);
	error = PF_DestroyFile(new_filename);
	if (error != PFE_OK)
		PF_ErrorHandler(error);

	printf("Index : %s has been destroyed\n", new_filename);
	free(new_filename);
	return AME_OK;
}

int AM_OpenIndex(char *fileName, int indexNo){
	int error, pffd, fileDesc;
	AMitab_ele *pt;
	char *headerbuf;
	char* new_filename;
	
	/*Initialisation */
	new_filename = malloc(sizeof(fileName) + sizeof(int));
	printf("length %d", AMitab_length);
	/*parameters cheking */
	if( AMitab_length >= AM_ITAB_SIZE){
		return AME_FULLTABLE;
	}
	sprintf(new_filename, "%s.%i", fileName, indexNo);
	pffd = PF_OpenFile(new_filename);
	if(pffd < 0){
		PF_ErrorHandler(pffd);
	}

	/* read the header which are stored on the second page of the file (index = 1) */ 
	error = PF_GetThisPage(pffd, 1, &headerbuf);
	if(error != PFE_OK){
		PF_ErrorHandler(error);
	}


	/* Fill the array of the AM index table */
	
	pt = AMitab + AMitab_length;
	
	pt->fd = pffd;
	pt->valid = TRUE;
	pt->dirty = FALSE;
	
	memcpy(pt->iname, new_filename, sizeof(new_filename));
	free(new_filename);
	memcpy( (int*) &pt->header.indexNo,(char*) (headerbuf), sizeof(int));
	memcpy((int*) &pt->header.attrType,(char*) (headerbuf + sizeof(int)),  sizeof(char));
	memcpy( (int*) &pt->header.attrLength, (char*) (headerbuf + sizeof(int) + sizeof(char)),sizeof(int));
	memcpy((int*) &pt->header.height_tree,(char*) (headerbuf + 2*sizeof(int) + sizeof(char)),  sizeof(int));
	memcpy( (int*) &pt->header.nb_leaf,(char*) (headerbuf + 3*sizeof(int) + sizeof(char)), sizeof(int));
	memcpy((int*) &pt->header.racine_page, (char*) (headerbuf + 4*sizeof(int) + sizeof(char)), sizeof(int));
	memcpy((int*) &pt->header.num_pages, (char*) (headerbuf + 5*sizeof(int) + sizeof(char)), sizeof(int));

	pt->fanout = ( (PF_PAGE_SIZE ) - 2*sizeof(int) - sizeof(bool_t)) / (sizeof(int) + pt->header.attrLength) + 1;
	pt->fanout_leaf = ( (PF_PAGE_SIZE - 3*sizeof(int) -sizeof(bool_t) ) / (pt->header.attrLength + sizeof(RECID)) ) + 1; /* number of recid into a leaf page */



	/*increment the size of the table*/
	AMitab_length ++;
	fileDesc = AMitab_length -1 ;
	/*unpin and touch the header page */
	error = PF_UnpinPage(pt->fd, 1, 1);
	if(error != PFE_OK){
		PF_ErrorHandler(error);
	}
	
	return fileDesc;
	
}

int AM_CloseIndex(int fileDesc){

	AMitab_ele* pt;
	int error;
	char* headerbuf;
	int i;
	i=0;
	
	

	if (fileDesc < 0 || fileDesc >= AMitab_length) return AME_FD;
	pt=AMitab + fileDesc ;
	/*check is file is not already close */
	if (pt->valid!=TRUE) return AME_INDEXNOTOPEN;


	/* check the header */
	if (pt->dirty==TRUE){ /* header has to be written again */
		error=PF_GetThisPage( pt->fd, 1, &headerbuf);
		if(error!=PFE_OK)PF_ErrorHandler(error);


		/* write the header  */
		memcpy((char*) (headerbuf), (int*) &pt->header.indexNo, sizeof(int));
		memcpy((char*) (headerbuf + sizeof(int)), (int*) &pt->header.attrType, sizeof(char));
		memcpy((char*) (headerbuf + sizeof(int) + sizeof(char)), (int*) &pt->header.attrLength, sizeof(int));
		memcpy((char*) (headerbuf + 2*sizeof(int) + sizeof(char)), (int*) &pt->header.height_tree, sizeof(int));
		memcpy((char*) (headerbuf + 3*sizeof(int) + sizeof(char)), (int*) &pt->header.nb_leaf, sizeof(int));
		memcpy((char*) (headerbuf + 4*sizeof(int) + sizeof(char)), (int*) &pt->header.racine_page, sizeof(int));
		memcpy((char*) (headerbuf + 5*sizeof(int) + sizeof(char)), (int*) &pt->header.num_pages, sizeof(int));

		/* the page is dirty now ==> last arg of PF_UnpinPage ("dirty") set to one */
		error = PF_UnpinPage(pt->fd, 1, 1);
		if(error != PFE_OK) PF_ErrorHandler(error);
		
		
	}
	/* close the file using pf layer */
	error=PF_CloseFile(pt->fd);
	if(error!=PFE_OK)PF_ErrorHandler(error);

	/* check that there is no scan in progress involving this file*/
	/* by scanning the scan table */
	/*****************for (i=0;i<AMscantab_length;i++){
			if((AMcantab+i)->AMfd==fileDesc) return AMESCANOPEN;
	}
	*/

	
	/*deletion */
	/* a file can be deleted only if it is at then end of the table in order to have static file descriptor */
	/* In any case the boolean valid is set to false to precise that this file is closed */
        pt->valid==FALSE;
	if(fileDesc==(AMitab_length-1)){ /* it is the last file of the table */ 
		AMitab_length--;

		if(AMitab_length >0){
			AMitab_length--;
			while (AMitab_length>0 &&  ((AMitab+ AMitab_length-1)->valid==FALSE)){
				AMitab_length--; /* delete all the closed file, which are the end of the table */
			}
		}
	}
	

	return AME_OK;
}



/* return AME_OK if succeed
 *
 */
int AM_InsertEntry(int fileDesc, char *value, RECID recId){
	int leafNum;
	int root_pagenum;
	int pos;
	int couple_size;
	int num_keys;
	int first_leaf;
	int last_leaf;
	int error;
	bool_t is_leaf;
	int offset;
	char attrType;
	int len_visitedNode;
	int* visitedNode; /* array of node visited : [root, level1, , ] */
	char* pagebuf;
	AMitab_ele* pt;
	char *tempbuffer;

	pt = AMitab + fileDesc;
	/* CASE : NO ROOT */
	if (pt->header.racine_page == -1){
		/* Create a tree */
		error = PF_AllocPage(pt->fd, &root_pagenum, &pagebuf);
		if (error != PFE_OK)
			PF_ErrorHandler(error);

		/* Write the node into the buffer: what kind of node */
		offset = 0;
		is_leaf = TRUE;
		num_keys = 1;
		memcpy((char*)(pagebuf + offset),(bool_t *) &is_leaf, sizeof(bool_t)); /* is leaf */
		offset += sizeof(bool_t);
		memcpy((char*)(pagebuf + offset),(int *) &num_keys, sizeof(int)); /* Number of key*/
		offset += sizeof(int);
		first_leaf = FIRST_LEAF;
		memcpy((char*)(pagebuf + offset ),(int *) &first_leaf, sizeof(int)); /* previous leaf pagenum */
		/* sizeof( recid) : space for LAST RECID */
		offset += sizeof(int);
		last_leaf = LAST_LEAF;
		memcpy((char*)(pagebuf + offset),(int *) &last_leaf, sizeof(int)); /* next leaf pagenum */
		offset += sizeof(int);
		memcpy((char*)(pagebuf + offset), (RECID*) &recId, sizeof(RECID));
		offset += sizeof(RECID);
		
		switch (pt->header.attrType){
			case 'c':
				memcpy((char*)(pagebuf + offset), (char*) value, pt->header.attrLength);
				break;
			case 'i':
				memcpy((char*)(pagebuf + offset), (int*) value, pt->header.attrLength);
				break;
			case 'f':
				memcpy((char*)(pagebuf + offset), (float*) value, pt->header.attrLength);
				break;
			default:
				return AME_INVALIDATTRTYPE;
				break;

		}

		/* change the AMi_elem */
		pt->header.racine_page = root_pagenum;
		pt->header.height_tree = 1;
		pt->header.nb_leaf = 1;
		pt->header.num_pages++;
		pt->dirty = TRUE;

		/* Unpin page */
		error = PF_UnpinPage(pt->fd, root_pagenum, 1);
		if (error != PFE_OK)
			PF_ErrorHandler(error);

		printf("root created on page : %d\n",root_pagenum);
		return AME_OK;
	}


	/* FIND LEAF */
	visitedNode = malloc( pt->header.height_tree * sizeof(int));
	
	/* pos < 0 : error,  pos >= 0 : position where to insert on the page */
	pos = AM_FindLeaf(fileDesc, value, visitedNode);
	
	if(pos < 0)
		return AME_KEYNOTFOUND;
	len_visitedNode = pt->header.height_tree;
	leafNum = visitedNode[len_visitedNode];
	
	/*leafNum = 2; */

	error = PF_GetThisPage(pt->fd, leafNum , &pagebuf);
	if (error != PFE_OK)
		PF_ErrorHandler(error);

	memcpy((int*) &num_keys, (char*)(pagebuf + sizeof(bool_t)), sizeof(int));

	couple_size = sizeof(RECID) + pt->header.attrLength;
	
	/* CASE : STILL SOME PLACE */
	if( num_keys < pt->fanout_leaf - 1){
		printf("Still some space into the leaf\n");
		/* Insert into leaf without splitting */
		offset = sizeof(bool_t) + 3 * sizeof(int) + pos*couple_size;
		/* ex : insert 3 
		 * num_keys = 4
		 * pos = 2 
		 * |(,1)|(,2)|(,4)|(,5)| | |  
		 * src = headerbuf + sizeofheader + pos*sizeofcouple
		 * dest = src + sizeofcouple
		 * size = (num_keys - pos) * (sizeofcouple)
		 with sizeof couple = sizeof recid + attrLength
		 */
		
		memmove((char*) (pagebuf + offset + couple_size), (char*) (pagebuf + offset), couple_size*(num_keys - pos));
		memcpy((char *) (pagebuf + offset ), (RECID*) &recId, sizeof(RECID));
		offset += sizeof(RECID);
		
		switch (pt->header.attrType){
			case 'c':
				memcpy((char*)(pagebuf + offset), (char*) value, pt->header.attrLength);
				break;
			case 'i':
				memcpy((char*)(pagebuf + offset), (int*) value, pt->header.attrLength);
				break;
			case 'f':
				memcpy((char*)(pagebuf + offset), (float*) value, pt->header.attrLength);
				break;
			default:
				return AME_INVALIDATTRTYPE;
				break;
		}

		num_keys++;
		memcpy((char*)(pagebuf + sizeof(bool_t)), (int*) &num_keys, sizeof(int));

		error = PF_UnpinPage(pt->fd, leafNum , 1);
		if (error != PFE_OK)
			PF_ErrorHandler(error);

		printf("record created");
		return AME_OK;

	}
	/* CASE : NO MORE PLACE */
	else{
		printf("No more space in the leaf\n");
		
		tempbuffer = malloc(couple_size*pt->fanout_leaf);
		offset = sizeof(bool_t) + 3 * sizeof(int);

		/* cpy in the buffer the part of the node before the key , then the key, then the part after */
		memcpy((char*) tempbuffer, (char*) (pagebuf + offset), couple_size*pos);
		offset += couple_size * pos;
		/*
		memcpy((char*) (tempbuffer + offset), (char*) (pagebuf + offset), couple_size*pos); /*cpy the couple */
/*
		memcpy((char*) (pagebuf + offset + couple_size), (char*) tempbuffer, couple_size*(num_keys - pos));
		*/
		free(tempbuffer);
		
		/* Insert into leaf after splitting */
		/* ALGO
		 *  Create a tempbuffer with the inserted value
		 *  copy from the pos given to a temp pointer the value splitted
		 * 	get the key to move up 
		 *  Should we reset the part of the node which is moved ?
		 * 	alloc page to create new leaf to fill with the temp pointers
		 *	update leaf header
		 *	update new_leaf header
		 *  new leaf->prev = pagenum of leaf
		 * 	leaf -> next = pagenum of new leaf
		 *	free the temp pointer
		 * 	unpin both nodes
		 *  update the AMiele header (num_pages, height_tree?, nb_leaf)
		 * 	
		
		visitedNode[ len_visitedNode ] = leaf
		visitedNode[i] = internal node
		visitedNode[0] = root
		for (i = len_visitedNode-1; i > 0; i++){
			
		}
		loop on : visitedNode 
		 * 	insert into parent(parent pagenum, key, left leaf, right leaf):
		 (recursif is way better)
		 		if no parent (ie the leaf was the root)
		 			get a page to store the internal node
		 			write the header of the internal node
		 			write the key in the internal node
		 			update the AMiele header (num_pages, height_tree ?, racine_page)
					unpin the page

		 */


	}



	return AME_OK;
}

int AM_DeleteEntry(int fileDesc, char *value, RECID recId){
	return AME_OK;
}

int AM_OpenIndexScan(int fileDesc, int op, char *value){
	return AME_OK;
}

RECID AM_FindNextEntry(int scanDesc){
	RECID res;
	res.recnum = 0;
	res.pagenum = 0;

	return res;
}

int AM_CloseIndexScan(int scanDesc){
	return AME_OK;
}

void AM_PrintError(char *errString){
	printf("%s\n",errString);
}
