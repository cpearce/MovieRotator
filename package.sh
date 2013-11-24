EXTRA_PREPROCESSOR_DEFINES=MOVIE_ROTATOR_VERSION=`git log -n1 --pretty=format:"\"Commit: %h\""` MSBuild.exe //p:Configuration=Release
/c/Program\ Files\ \(x86\)/Inno\ Setup\ 5/ISCC.exe Installer/MovieRotator-Installer.iss
