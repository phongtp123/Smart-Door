# Custom Zephyr funtion that imports the button/ subdirectory if the Kconfig
# option CUSTOM_XXX is defined (XXX is device that have custom driver implemented)
add_subdirectory_ifdef(CONFIG_CUSTOM_BH1750 bh1750)
add_subdirectory_ifdef(CONFIG_CUSTOM_RC522 rc522)
