FIND_LIBRARY(freenect2_LIBRARY freenect2
    PATHS C:/Users/figlab/Vivian_Workspace/libfreenect2/buildex/install/lib
    NO_DEFAULT_PATH
)
SET(freenect2_LIBRARIES ${freenect2_LIBRARY} )
FIND_PATH(freenect2_INCLUDE_DIR libfreenect2/libfreenect2.hpp
    PATHS C:/Users/figlab/Vivian_Workspace/libfreenect2/buildex/install/include
    NO_DEFAULT_PATH
)
SET(freenect2_INCLUDE_DIRS ${freenect2_INCLUDE_DIR})
