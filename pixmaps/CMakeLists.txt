SET (PIXMAPS
	    cancel.png
	    hibernate.png
	    lock.png
	    logo.png
	    logout.png
	    restart.png
	    shutdown.png
	    suspend.png)
INSTALL (FILES ${PIXMAPS} DESTINATION share/pixmaps/${PROJECT})
MESSAGE(STATUS "Images will be installed in ${CMAKE_INSTALL_PREFIX}/share/pixmaps/${PROJECT}")
