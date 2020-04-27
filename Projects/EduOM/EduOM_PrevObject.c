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
 * Module: EduOM_PrevObject.c
 *
 * Description: 
 *  Return the previous object of the given current object.
 *
 * Exports:
 *  Four EduOM_PrevObject(ObjectID*, ObjectID*, ObjectID*, ObjectHdr*)
 */

#include "EduOM_common.h"
#include "BfM.h"
#include "EduOM_Internal.h"

/*@================================
 * EduOM_PrevObject()
 *================================*/
/*
 * Function: Four EduOM_PrevObject(ObjectID*, ObjectID*, ObjectID*, ObjectHdr*)
 *
 * Description: 
 * (Following description is for original ODYSSEUS/COSMOS OM.
 *  For ODYSSEUS/EduCOSMOS EduOM, refer to the EduOM project manual.)
 *
 *  Return the previous object of the given current object. Find the object in
 *  the same page which has the current object and  if there  is no previous
 *  object in the same page, find it from the previous page.
 *  If the current object is NULL, return the last object of the file.
 *
 * Returns:
 *  error code
 *    eBADCATALOGOBJECT_OM
 *    eBADOBJECTID_OM
 *    some errors caused by function calls
 *
 * Side effect:
 *  1) parameter prevOID
 *     prevOID is filled with the previous object's identifier
 *  2) parameter objHdr
 *     objHdr is filled with the previous object's header
 */
Four EduOM_PrevObject(
    ObjectID *catObjForFile, /* IN informations about a data file */
    ObjectID *curOID,        /* IN a ObjectID of the current object */
    ObjectID *prevOID,       /* OUT the previous object of a current object */
    ObjectHdr *objHdr)       /* OUT the object header of previous object */
{
    /* These local variables are used in the solution code. However, you don��t have to use all these variables in your code, and you may also declare and use additional local variables if needed. */
    Four e;                         /* error */
    Two i;                          /* index */
    Four offset;                    /* starting offset of object within a page */
    PageID pid;                     /* a page identifier */
    PageNo pageNo;                  /* a temporary var for previous page's PageNo */
    SlottedPage *apage;             /* a pointer to the data page */
    Object *obj;                    /* a pointer to the Object */
    SlottedPage *catPage;           /* buffer page containing the catalog object */
    sm_CatOverlayForData *catEntry; /* overlay structure for catalog object access */

    /*@ parameter checking */
    if (catObjForFile == NULL)
        ERR(eBADCATALOGOBJECT_OM);

    if (prevOID == NULL)
        ERR(eBADOBJECTID_OM);

    e = BfM_GetTrain((TrainID *)catObjForFile, &catPage, PAGE_BUF);
    if (e < 0)
        ERR(e);

    GET_PTR_TO_CATENTRY_FOR_DATA(catObjForFile, catPage, catEntry);

    //if curOID is NULL
    if (curOID == NULL)
    {
        //get the first object
        pid.volNo = catObjForFile->volNo;
        pid.pageNo = catEntry->lastPage;
        pageNo = pid.pageNo;

        //get the last obj in the slot array of the last page
        while (pageNo != NULL)
        {
            e = BfM_GetTrain((TrainID *)&pid, &apage, PAGE_BUF);
            if (e < 0)
                ERR(e);

            for (i = apage->header.nSlots - 1; i >= 0; i--)
            {
                if (apage->slot[-i].offset != EMPTYSLOT)
                { //found
                    offset = apage->slot[-i].offset;
                    obj = (Object *)&(apage->data[offset]);

                    //get the previous object
                    MAKE_OBJECTID(*prevOID, pid.volNo, pid.pageNo, i, apage->slot[-i].unique);
                    objHdr = &(obj->header);

                    e = BfM_FreeTrain((TrainID *)catObjForFile, PAGE_BUF);
                    if (e < 0)
                        ERR(e);

                    e = BfM_FreeTrain((TrainID *)&pid, PAGE_BUF);
                    if (e < 0)
                        ERR(e);

                    return e;
                }
            }
            //current page has no object. Get the previous page
            pageNo = apage->header.prevPage;

            e = BfM_FreeTrain((TrainID *)&pid, PAGE_BUF);
            if (e < 0)
                ERR(e);

            MAKE_PAGEID(pid, catObjForFile->volNo, pid.pageNo);
        }
        //if the code reached here, the file is empty.
        //return EOS (End of Scan)
    }
    //if curOID is not NULL
    else
    {
        pid.pageNo = curOID->pageNo;
        pid.volNo = curOID->volNo;

        //search for object with curOID
        pageNo = curOID->pageNo;

        int initLoop = 1;

        while (pageNo != NULL)
        {
            e = BfM_GetTrain((TrainID *)&pid, &apage, PAGE_BUF);
            if (e < 0)
                ERR(e);

            if (initLoop)
            { //first time in the loop
                i = curOID->slotNo - 1;
                initLoop = 0;
            }
            else
            {
                i = apage->header.nSlots - 1;
            }

            for (; i >= 0; i--)
            {
                if (apage->slot[-i].offset != EMPTYSLOT)
                {
                    //found
                    offset = apage->slot[-i].offset;
                    obj = (Object *)&(apage->data[offset]);
                    MAKE_OBJECTID(*prevOID, pid.volNo, pid.pageNo, i, apage->slot[-i].unique);
                    objHdr = &obj->header;

                    e = BfM_FreeTrain((TrainID *)catObjForFile, PAGE_BUF);
                    if (e < 0)
                        ERR(e);

                    e = BfM_FreeTrain((TrainID *)&pid, PAGE_BUF);
                    if (e < 0)
                        ERR(e);

                    return e;
                }
            }
            //page empty, move on to the next page
            pageNo = apage->header.prevPage;

            e = BfM_FreeTrain((TrainID *)&pid, PAGE_BUF);
            if (e < 0)
                ERR(e);

            MAKE_PAGEID(pid, catObjForFile->volNo, pageNo);
        }
        //if the code reached here, given object is the last object of the last page
        //return EOS
    }

    e = BfM_FreeTrain((TrainID *)catObjForFile, PAGE_BUF);
    if (e < 0)
        ERR(e);

    return (EOS);

} /* EduOM_PrevObject() */
