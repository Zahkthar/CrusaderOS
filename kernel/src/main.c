#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <limine.h>

#include "asm.h"
#include "graphics/framebuffer.h"
#include "graphics/colors.h"

static volatile LIMINE_BASE_REVISION(1);

static volatile struct limine_framebuffer_request framebuffer_request =
    {.id = LIMINE_FRAMEBUFFER_REQUEST,
     .revision = 0};

void kernel_main(void)
{
    if (LIMINE_BASE_REVISION_SUPPORTED == false)
    {
        hcf();
    }

    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1)
    {
        hcf();
    }

    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    framebuffer_fillRect(framebuffer, (Rect){50, 50, 100, 100}, RGB_GREEN);
    framebuffer_drawRect(framebuffer, (Rect){250, 50, 100, 100}, RGB_RED);

    framebuffer_drawLine(framebuffer, (Vec2){500, 500},(Vec2){500 + 50, 500 - 50}, RGB_CYAN);
    framebuffer_drawLine(framebuffer, (Vec2){500, 500},(Vec2){500 + 50, 500}     , RGB_CYAN);
    framebuffer_drawLine(framebuffer, (Vec2){500, 500},(Vec2){500 + 50, 500 + 50}, RGB_CYAN);
    framebuffer_drawLine(framebuffer, (Vec2){500, 500},(Vec2){500     , 500 + 50}, RGB_CYAN);
    framebuffer_drawLine(framebuffer, (Vec2){500, 500},(Vec2){500 - 50, 500 + 50}, RGB_CYAN);
    framebuffer_drawLine(framebuffer, (Vec2){500, 500},(Vec2){500 - 50, 500}     , RGB_CYAN);
    framebuffer_drawLine(framebuffer, (Vec2){500, 500},(Vec2){500 - 50, 500 - 50}, RGB_CYAN);
    framebuffer_drawLine(framebuffer, (Vec2){500, 500},(Vec2){500     , 500 - 50}, RGB_CYAN);

    hcf();
}
