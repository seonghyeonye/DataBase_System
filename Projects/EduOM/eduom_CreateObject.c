/******************************************************************************/
/*                                                                            */
/*    Copyright (c) 2013-2015, Kyu-Young Whang, KAIST                         */
/*    All rights reserved.                                                    */
/*                                                                            */
/*    Redistribution and use in source and binary forms, with or without      */
/*    modification, are permitted provided that the following conditions      */
/*    are met:                                                                */
/*                                                                            */
/*    1. Redistributions of source code must retain the above copyright       */
/*       notice, this list of conditions and the following disclaimer.        */
/*                                                                            */
/*    2. Redistributions in binary form must reproduce the above copyright    */
/*       notice, this list of conditions and the following disclaimer in      */
/*       the documentation and/or other materials provided with the           */
/*       distribution.                                                        */
/*                                                                            */
/*    3. Neither the name of the copyright holder nor the names of its        */
/*       contributors may be used to endorse or promote products derived      */
/*       from this software without specific prior written permission.        */
/*                                                                            */
/*    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS     */
/*    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT       */
/*    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS       */
/*    FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE          */
/*    COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,    */
/*    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,    */
/*    BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;        */
/*    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER        */
/*    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT      */
/*    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN       */
/*    ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE         */
/*    POSSIBILITY OF SUCH DAMAGE.                                             */
/*                                                                            */
/******************************************************************************/
/******************************************************************************/
/*                                                                            */
/*    ODYSSEUS/EduCOSMOS Educational Purpose Object Storage System            */
/*    (Version 1.0)                                                           */
/*                                                                            */
/*    Developed by Professor Kyu-Young Whang et al.                           */
/*                                                                            */
/*    Advanced Information Technology Research Center (AITrc)                 */
/*    Korea Advanced Institute of Science and Technology (KAIST)              */
/*                                                                            */
/*    e-mail: odysseus.educosmos@gmail.com                                    */
/*                                                                            */
/******************************************************************************/
/*
 * Module : eduom_CreateObject.c
 * 
 * Description :
 *  eduom_CreateObject() creates a new object near the specified object.
 *
 * Exports:
 *  Four eduom_CreateObject(ObjectID*, ObjectID*, ObjectHdr*, Four, char*, ObjectID*)
 */


#include <string.h>
#include "EduOM_common.h"
#include "RDsM.h"		/* for the raw disk manager call */
#include "BfM.h"		/* for the buffer manager call */
#include "EduOM_Internal.h"



/*@================================
 * eduom_CreateObject()
 *================================*/
/*
 * Function: Four eduom_CreateObject(ObjectID*, ObjectID*, ObjectHdr*, Four, char*, ObjectID*)
 * 
 * Description :
 * (Following description is for original ODYSSEUS/COSMOS OM.
 *  For ODYSSEUS/EduCOSMOS EduOM, refer to the EduOM project manual.)
 *
 *  eduom_CreateObject() creates a new object near the specified object; the near
 *  page is the page holding the near object.
 *  If there is no room in the near page and the near object 'nearObj' is not
 *  NULL, a new page is allocated for object creation (In this case, the newly
 *  allocated page is inserted after the near page in the list of pages
 *  consiting in the file).
 *  If there is no room in the near page and the near object 'nearObj' is NULL,
 *  it trys to create a new object in the page in the available space list. If
 *  fail, then the new object will be put into the newly allocated page(In this
 *  case, the newly allocated page is appended at the tail of the list of pages
 *  cosisting in the file).
 *
 * Returns:
 *  error Code
 *    eBADCATALOGOBJECT_OM
 *    eBADOBJECTID_OM
 *    some errors caused by fuction calls
 */
Four eduom_CreateObject(
    ObjectID	*catObjForFile,	/* IN file in which object is to be placed */
    ObjectID 	*nearObj,	/* IN create the new object near this object */
    ObjectHdr	*objHdr,	/* IN from which tag & properties are set */
    Four	length,		/* IN amount of data */
    char	*data,		/* IN the initial data for the object */
    ObjectID	*oid)		/* OUT the object's ObjectID */
{
	/* These local variables are used in the solution code. However, you don��t have to use all these variables in your code, and you may also declare and use additional local variables if needed. */
    Four        e;		/* error number */
    Four	neededSpace;	/* space needed to put new object [+ header] */
    SlottedPage *apage, *lastpage;		/* pointer to the slotted page buffer */
    Four        alignedLen;	/* aligned length of initial data */
    Boolean     needToAllocPage;/* Is there a need to alloc a new page? */
    PageID      pid;            /* PageID in which new object to be inserted */
    PageID      nearPid;
    Four        firstExt;	/* first Extent No of the file */
    Object      *obj;		/* point to the newly created object */
    Two         i;		/* index variable */
    sm_CatOverlayForData *catEntry; /* pointer to data file catalog information */
    SlottedPage *catPage;	/* pointer to buffer containing the catalog */
    FileID      fid;		/* ID of file where the new object is placed */
    Two         eff;		/* extent fill factor of file */
    Boolean     isTmp;
    PhysicalFileID pFid;
    Four wholeDatasize;
    ShortPageID selectedAvaillist;
    Object      object;
    Four        numSlot;
    PageID      lastpid;
    

    /*@ parameter checking */
    
    if (catObjForFile == NULL) ERR(eBADCATALOGOBJECT_OM);

    if (objHdr == NULL) ERR(eBADOBJECTID_OM);
    
    /* Error check whether using not supported functionality by EduOM */
    if(ALIGNED_LENGTH(length) > LRGOBJ_THRESHOLD) ERR(eNOTSUPPORTED_EDUOM);

    alignedLen=ALIGNED_LENGTH(length);
    neededSpace=sizeof(ObjectHdr)+alignedLen+sizeof(SlottedPageSlot);
    BfM_GetTrain((TrainID *)catObjForFile,(char**)&catPage,PAGE_BUF);
    GET_PTR_TO_CATENTRY_FOR_DATA(catObjForFile,catPage,catEntry);
    wholeDatasize=PAGESIZE-SP_FIXED;
    selectedAvaillist=-1;

    if(nearObj==NULL){
        //for case 1 re
        if(neededSpace<0.2*wholeDatasize&&catEntry->availSpaceList10!=-1)
            selectedAvaillist=catEntry->availSpaceList10;
        else if(neededSpace<0.3*wholeDatasize&&catEntry->availSpaceList20!=-1)
            selectedAvaillist=catEntry->availSpaceList20;
        else if(neededSpace<0.4*wholeDatasize&&catEntry->availSpaceList30!=-1)
            selectedAvaillist=catEntry->availSpaceList30;
        else if(neededSpace<0.5*wholeDatasize&&catEntry->availSpaceList40!=-1)
            selectedAvaillist=catEntry->availSpaceList40;
        else if(neededSpace<0.5*wholeDatasize&&catEntry->availSpaceList50!=-1)
            selectedAvaillist=catEntry->availSpaceList50;

        //for case 2
        if(catEntry->lastPage!=-1){
            MAKE_PAGEID(lastpid,catEntry->fid.volNo,catEntry->lastPage);
            BfM_GetTrain(&pid,(char**)&lastpage,PAGE_BUF);
        }

        if(neededSpace<=0.5*wholeDatasize&&selectedAvaillist!=-1){
            MAKE_PAGEID(pid,catEntry->fid.volNo,selectedAvaillist);

            BfM_GetTrain(&pid,(char**)&apage,PAGE_BUF);
            om_RemoveFromAvailSpaceList(catObjForFile,&pid,apage);
            EduOM_CompactPage(apage,NIL);
            BfM_FreeTrain(&pid,PAGE_BUF);
        }
        else if(neededSpace<=SP_FREE(lastpage)){
            EduOM_CompactPage(lastpage,NIL);
            BfM_FreeTrain(&lastpid,PAGE_BUF);
        }
        else{
            MAKE_PHYSICALFILEID(pFid, catEntry->fid.volNo, catEntry->firstPage);
            RDsM_PageIdToExtNo((PageID *)&pFid, &firstExt);
            //null allowed?
            RDsM_AllocTrains(catEntry->fid.volNo, firstExt,NULL, catEntry->eff, 1,PAGESIZE2, &pid);
            BfM_GetNewTrain(&pid,(char **)&apage,PAGE_BUF);
        }
    }
    else{
        BfM_GetTrain((TrainID *)nearObj,(char**)&apage,PAGE_BUF);
        //condition right?(cfree vs free)
        if(neededSpace<=SP_FREE(apage)){
            MAKE_PAGEID(pid,nearObj->volNo,nearObj->pageNo);
            om_RemoveFromAvailSpaceList(catObjForFile,&pid,apage);
            //param NIL right?
            EduOM_CompactPage(apage,NIL);
        }
        else{
            MAKE_PHYSICALFILEID(pFid, catEntry->fid.volNo, catEntry->firstPage);
            RDsM_PageIdToExtNo((PageID *)&pFid, &firstExt);
            MAKE_PAGEID(nearPid,nearObj->volNo,nearObj->pageNo);
            RDsM_AllocTrains(catEntry->fid.volNo, firstExt, &nearPid, catEntry->eff, 1,PAGESIZE2, &pid);
            BfM_GetNewTrain(&pid,(char **)&apage,PAGE_BUF);
            apage->header.pid=pid;
            apage->header.nSlots=1;
            apage->header.free=0;
            apage->header.unused=0;
            apage->header.flags=SLOTTED_PAGE_TYPE;
            apage->header.fid=catEntry->fid;
            om_FileMapAddPage(catObjForFile,&nearPid,&pid);
            BfM_FreeTrain(&pid,PAGE_BUF);
        }
        BfM_FreeTrain(nearObj,PAGE_BUF);
    }

    objHdr->length=length;

    memcpy(apage->data+(apage->header.free),objHdr,sizeof(ObjectHdr));
    memcpy(apage->data+(apage->header.free)+sizeof(ObjectHdr),data,alignedLen);

    if(apage->header.free==0)
        i=apage->header.nSlots-1;
    else{
        i=apage->header.nSlots;
    }

    numSlot=i+1;

    //avilable space due to destroyed slot
    for(int j=0; j<apage->header.nSlots;j++){
        if(apage->slot[-j].offset==EMPTYSLOT){
            i=j;
            numSlot=apage->header.nSlots;
            break;
        }        
    }

    apage->slot[-i].offset=apage->header.free;
    om_GetUnique(&pid, &(apage->slot[-i].unique));

    apage->header.free+=alignedLen+sizeof(ObjectHdr);
    apage->header.nSlots=numSlot;

    om_PutInAvailSpaceList(catObjForFile,&pid,apage);

    oid->pageNo=apage->header.pid.pageNo;
    oid->volNo=apage->header.pid.volNo;
    oid->slotNo=i;
    oid->unique=apage->slot[-i].unique;

    BfM_FreeTrain(catObjForFile,PAGE_BUF);
    return(eNOERROR);
    
} /* eduom_CreateObject() */
