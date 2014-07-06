/**
 *  @file
 *  
 */


#include  "TransBitmap.h"


TransBitmap* transbitmap_create_with_resources(uint32_t residWhiteMask,
                                               uint32_t residBlackMask)
{

TransBitmap* pMyRet;

   pMyRet = (TransBitmap*) malloc(sizeof(TransBitmap));
   if (pMyRet == 0)
   {
      return 0;
   }

   pMyRet->pBmpWhiteMask = gbitmap_create_with_resource(residWhiteMask);
   pMyRet->pBmpBlackMask = gbitmap_create_with_resource(residBlackMask);

   if ((pMyRet->pBmpWhiteMask == 0) ||
       (pMyRet->pBmpBlackMask == 0))
   {
      //  incomplete init, so return nothing to show this.
      transbitmap_destroy(pMyRet);
      return 0;
   }

   return pMyRet;

}  /* end of transbitmap_create_with_resources */


void  transbitmap_destroy(TransBitmap *pTransBmp)
{

   if (pTransBmp == 0)
      return;

   if (pTransBmp->pBmpWhiteMask != 0)
   {
      gbitmap_destroy(pTransBmp->pBmpWhiteMask);
      pTransBmp->pBmpWhiteMask = 0;
   }

   if (pTransBmp->pBmpBlackMask != 0)
   {
      gbitmap_destroy(pTransBmp->pBmpBlackMask);
      pTransBmp->pBmpBlackMask = 0;
   }

   free(pTransBmp);

   return;

}  /* end of transbitmap_destroy */


void  transbitmap_draw_in_rect(TransBitmap *pTransBmp, GContext* ctx, GRect rect)
{

   //  Per this post by RenaudCazoulat
   //    http://forums.getpebble.com/discussion/comment/36006/#Comment_36006
   //  we want to composite our white mask using GCompOr
   //  and our black mask using GCompClear.

   graphics_context_set_compositing_mode(ctx, GCompOpOr);
   graphics_draw_bitmap_in_rect(ctx, pTransBmp->pBmpWhiteMask, rect);

   graphics_context_set_compositing_mode(ctx, GCompOpClear);
   graphics_draw_bitmap_in_rect(ctx, pTransBmp->pBmpBlackMask, rect);

   return;

}  /* end of transbitmap_draw_in_rect */




