# Per-tab image loading

The default status bar has an image dropdown before the zoom controls:

- **Show images** loads and displays images normally.
- **Cached images** displays images already available to the renderer, without
  starting new automatic image downloads.
- **No images** blocks new image downloads and hides images already displayed,
  including image backgrounds. Changing back restores their display.

The selected mode belongs to the current tab. It does not change the global
preference or create a website preference. The control follows the active tab
and updates when the image preference is changed through Quick Preferences.
It is disabled for internal pages such as preferences and history.

If the status bar has a saved custom layout, right-click it, choose
**Customize**, and add **Image Loading** to the toolbar. Existing custom layouts
are preserved. The control can also be placed on another toolbar.

QtWebEngine's cached mode uses Blink's renderer memory cache. Images present
only in the disk HTTP cache may remain unloaded, particularly after restarting
the browser. The Qt public request interception API does not expose a per-image
cache-only network load flag. This is not full Opera 12 disk-cache parity.
The image context menu's explicit reload remains available in restricted modes.
On Qt 6.8 and newer, immediate visibility updates also reach existing subframes;
older Qt builds apply immediate changes to the main frame and inject the mode
into subframes as they load.

## Validation

Build the local Qt 6.12 application:

```sh
cmake --build build-qt612 -j 2
node tests/imageVisibility.test.js
./run-otter-qt612.sh
```

Manual checks in a disposable profile:

1. Open a page with ordinary images, image backgrounds, and an iframe.
2. Select **No images**. Existing images should disappear without reloading the
   page or discarding form input. Images inserted later should also be hidden.
3. Select **Cached images**. Already loaded images should reappear; newly added
   uncached image URLs should not download. Test with the developer tools
   network cache enabled.
4. Select **Show images**. Pending automatic images should load normally.
5. Switch between two tabs with different modes. The label and checked menu
   item should follow the active tab, and changes must not affect the other tab.
6. Change Images in Quick Preferences and confirm the status label updates.
7. Navigate, reload, open an internal page, and close the active tab. Confirm the
   control follows the resulting page and is disabled when it is not a web tab.

The JavaScript regression checks reversible visibility, document isolation,
repeated updates, and changing the mode before DOMContentLoaded. Chromium
startup and local HTTP test servers are blocked by the restricted tool shell,
so live browser/network checks must be run outside that shell.
