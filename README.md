* For compiling the driver: 
`make CROSS_COMPILE=arm-linux-gnueabihf- ARCH=arm KDIR=../linux-5.15.6/build/`

* For compiling the testing program:
`arm-linux-gnueabihf-gcc main.c -static -o main`

* For starting the simulation:
./qemu-system-arm -nographic \
  -machine vexpress-a9 \
  -kernel linux-5.15.6/build/arch/arm/boot/zImage \
  -dtb linux-5.15.6/build/arch/arm/boot/dts/vexpress-v2p-ca9.dtb \
  -initrd initramfs_busybox/initramfs.gz \
  -fsdev local,path=pilote_i2c,security_model=mapped,id=mnt \
  -device virtio-9p-device,fsdev=mnt,mount_tag=mnt

* For inserting the "\mnt" folder:
`mount -t 9p -o trans=virtio mnt /mnt -oversion=9p2000.L,msize=10240`