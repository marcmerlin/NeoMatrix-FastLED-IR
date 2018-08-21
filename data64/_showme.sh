for i in *; do echo $i; mplayer -osdlevel 0 -x 1024 -y 1024 $i; alarm 1 bash -c read; done
