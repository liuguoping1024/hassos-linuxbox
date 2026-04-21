# Fix script permissions after kernel source extraction + patching.
# Amlogic SDK patches add .sh files that lose +x when distributed as tarballs.

define LINUX_FIX_SCRIPT_PERMISSIONS
	find $(@D)/scripts -name '*.sh' -exec chmod +x {} +
	find $(@D)/scripts -name '*.pl' -exec chmod +x {} +
endef

LINUX_POST_PATCH_HOOKS += LINUX_FIX_SCRIPT_PERMISSIONS
