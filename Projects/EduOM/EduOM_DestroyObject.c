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
 * Module : EduOM_DestroyObject.c
 * 
 * Description : 
 *  EduOM_DestroyObject() destroys the specified object.
 *
 * Exports:
 *  Four EduOM_DestroyObject(ObjectID*, ObjectID*, Pool*, DeallocListElem*)
 */

#include "EduOM_common.h"
#include "Util.h" /* to get Pool */
#include "RDsM.h"
#include "BfM.h" /* for the buffer manager call */
#include "LOT.h" /* for the large object manager call */
#include "EduOM_Internal.h"

/*@================================
 * EduOM_DestroyObject()
 *================================*/
/*
 * Function: Four EduOM_DestroyObject(ObjectID*, ObjectID*, Pool*, DeallocListElem*)
 * 
 * Description : 
 * (Following description is for original ODYSSEUS/COSMOS OM.
 *  For ODYSSEUS/EduCOSMOS EduOM, refer to the EduOM project manual.)
 *
 *  (1) What to do?
 *  EduOM_DestroyObject() destroys the specified object. The specified object
 *  will be removed from the slotted page. The freed space is not merged
 *  to make the contiguous space; it is done when it is needed.
 *  The page's membership to 'availSpaceList' may be changed.
 *  If the destroyed object is the only object in the page, then deallocate
 *  the page.
 *
 *  (2) How to do?
 *  a. Read in the slotted page
 *  b. Remove this page from the 'availSpaceList'
 *  c. Delete the object from the page
 *  d. Update the control information: 'unused', 'freeStart', 'slot offset'
 *  e. IF no more object in this page THEN
 *	   Remove this page from the filemap List
 *	   Dealloate this page
 *    ELSE
 *	   Put this page into the proper 'availSpaceList'
 *    ENDIF
 * f. Return
 *
 * Returns:
 *  error code
 *    eBADCATALOGOBJECT_OM
 *    eBADOBJECTID_OM
 *    eBADFILEID_OM
 *    some errors caused by function calls
 */
Four EduOM_DestroyObject(
    ObjectID *catObjForFile, /* IN file containing the object */
    ObjectID *oid,           /* IN object to destroy */
    Pool *dlPool,            /* INOUT pool of dealloc list elements */
    DeallocListElem *dlHead) /* INOUT head of dealloc list */
{
    /* These local variables are used in the solution code. However, you don��t have to use all these variables in your code, and you may also declare and use additional local variables if needed. */
    Four e;                         /* error number */
    Two i;                          /* temporary variable */
    FileID fid;                     /* ID of file where the object was placed */
    PageID pid;                     /* page on which the object resides */
    SlottedPage *apage;             /* pointer to the buffer holding the page */
    Four offset;                    /* start offset of object in data area */
    Object *obj;                    /* points to the object in data area */
    Four alignedLen;                /* aligned length of object */
    Boolean last;                   /* indicates the object is the last one */
    SlottedPage *catPage;           /* buffer page containing the catalog object */
    sm_CatOverlayForData *catEntry; /* overlay structure for catalog object access */
    DeallocListElem *dlElem;        /* pointer to element of dealloc list */
    PhysicalFileID pFid;            /* physical ID of file */

    /*@ Check parameters. */
    if (catObjForFile == NULL)
        ERR(eBADCATALOGOBJECT_OM);

    if (oid == NULL)
        ERR(eBADOBJECTID_OM);

    //get the page containing the catalog object
    e = BfM_GetTrain((TrainID *)catObjForFile, (char **)&catPage, PAGE_BUF);
    if (e < 0)
        ERR(e);

    //get the object to destroy
    pid.pageNo = oid->pageNo;
    pid.volNo = oid->volNo;

    //get the page that contains the object to destroy
    e = BfM_GetTrain((TrainID *)&pid, (char **)&apage, PAGE_BUF);
    if (e < 0)
        ERR(e);

    if (oid->slotNo == (apage->header.nSlots - 1))
    {
        last = 1;
    }
    else
    {
        last = 0;
    }

    //remove the page that contains the object to destroy
    e = om_RemoveFromAvailSpaceList(catObjForFile, &pid, apage);
    if (e < 0)
        ERRB1(e, &pid, PAGE_BUF);

    //get the object to destroy
    offset = apage->slot[-(oid->slotNo)].offset;
    obj = &(apage->data[offset]);

    //set the slot that contained the object to be empty
    apage->slot[-(oid->slotNo)].offset = EMPTYSLOT;

    //update the header
    if (last)
    { //if object was last, reduce the number of slots
        apage->header.nSlots--;
    }

    alignedLen = ALIGNED_LENGTH(obj->header.length);

    //if the object to be destroyed is the last object
    if (sizeof(ObjectHdr) + offset + alignedLen == apage->header.free)
    {
        //update free
        apage->header.free = offset;
    }
    else //if contiguous free space stays the same
    {
        //update unused
        //add size of the deleted object
        apage->header.unused = apage->header.unused + sizeof(ObjectHdr) + alignedLen;
    }

    //if the object was the only object in the page
    //and the page is not the first page of the file
    if (apage->header.free == 0)
    {

        e = om_FileMapDeletePage(catObjForFile, &pid);

        if (e < 0)
            ERRB1(catObjForFile, &pid, PAGE_BUF);

        e = Util_getElementFromPool(dlPool, &dlElem);

        if (e < 0)
            ERR(e);

        dlElem->type = DL_PAGE;
        dlElem->elem.pid = pid;
        dlElem->next = dlHead->next;
        dlHead->next = dlElem;
    }
    //if the object was not the only object in the page
    //or the page is the first page of the file
    else
    {
        e = om_PutInAvailSpaceList(catObjForFile, &pid, apage);
        if (e < 0)
            ERRB1(e, &pid, PAGE_BUF);
    }

    e = BfM_FreeTrain((TrainID *)catObjForFile, PAGE_BUF);
    if (e < 0)
        ERR(e);

    e = BfM_SetDirty((TrainID *)&pid, PAGE_BUF);
    if (e < 0)
        ERRB1(e, &pid, PAGE_BUF);

    e = BfM_FreeTrain((TrainID *)&pid, PAGE_BUF);
    if (e < 0)
        ERR(e);

    return (eNOERROR);

} /* EduOM_DestroyObject() */
