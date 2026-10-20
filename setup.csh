# Source from the extracted MOLLER-MD-framework directory (tcsh/csh).
if ( ! -d "$cwd/MOLLER-replay" || ! -d "$cwd/install/include" ) then
 echo "Run from MOLLER-MD-framework after building/installing MOLLER-offline."
else
 setenv MOLLER_OFFLINE "$cwd/install"
 setenv MOLLER_REPLAY "$cwd/MOLLER-replay"
 setenv DB_DIR "$MOLLER_REPLAY/DB"
 if ( $?LD_LIBRARY_PATH ) then
  setenv LD_LIBRARY_PATH "$MOLLER_OFFLINE/lib64:$LD_LIBRARY_PATH"
 else
  setenv LD_LIBRARY_PATH "$MOLLER_OFFLINE/lib64"
 endif
 if ( $?ROOT_INCLUDE_PATH ) then
  setenv ROOT_INCLUDE_PATH "$MOLLER_OFFLINE/include:$ROOT_INCLUDE_PATH"
 else
  setenv ROOT_INCLUDE_PATH "$MOLLER_OFFLINE/include"
 endif
 echo "MOLLER MD replay configured. DB_DIR=$DB_DIR"
endif
