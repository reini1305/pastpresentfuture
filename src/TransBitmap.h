/**
 *  @file
 *  
 */

#pragma once

#include  "pebble.h"


///  Carries all data needed to draw a "png-trans" bitmap resource.
typedef struct
{

   GBitmap* pBmpWhiteMask;
   GBitmap* pBmpBlackMask;

} TransBitmap;

GBitmap* gbitmap_create_with_resource(uint32_t resource_id);

/**
 *  Public means of instantiating TransBitmap.  We load the bitmaps needed to
 *  render a transparent image resource and return a pointer to the newly created
 *  carrier object.
 *  
 *  This interface automatically infers the _WHITE / _BLACK resource suffixes
 *  generated by pebble for a "png-trans" base resource type.  This is not well
 *  documented, but is described in this forum post:
 *  
 *     http://forums.getpebble.com/discussion/4596/transparent-png-support
 *  
 *  So our single argument is exactly the name shown for the desired "png-trans"
 *  resource in the appinfo.json resources / media section (but expressed as
 *  a manifest, not a string).
 */
#define transbitmap_create_with_resource_prefix(RESOURCE_ID_STEM_)  \
   transbitmap_create_with_resources(RESOURCE_ID_STEM_ ## _WHITE,   \
                                     RESOURCE_ID_STEM_ ## _BLACK)

///  Destroy a bitmap instance created using transbitmap_create_with_resource_prefix().
void  transbitmap_destroy(TransBitmap *pTransBmp);

/**
 *  Render our transparent bitmap into the specified context and rectangle.
 * 
 * @param pTransBmp Info about "png-trans" resource to be rendered.
 * @param ctx Graphics context to write into.  We leave this context's 
 *             compositing mode undefined.
 * @param rect Rectangle, within ctx, in which to render pTransBmp image.
 */
void  transbitmap_draw_in_rect(TransBitmap *pTransBmp, GContext* ctx, GRect rect);


/**
 *  Actual creation routine, use transbitmap_create_with_resource_prefix()
 *  instead of calling this directly.
 */
TransBitmap* transbitmap_create_with_resources(uint32_t residWhiteMask,
                                               uint32_t residBlackMask);

