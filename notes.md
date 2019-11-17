Learnings

1. Use squashfs-tools 4.4 for reproducibility features
2. Set SOURCE_DATE_EPOCH!!!! This reduced the number of patch blocks from lots to 6
3. Turn off fragments in mksquashfs. This reduced the compressed patch file size from 45K to 4.5K
4. Buildroot has xattrs enabled for mksquashfs. Since we don't use xattrs
   anyway, it seemed like a good idea to turn it off. This saves 1 patch block.
   Now there are 4. (No change patch bzip2'd is 3.7K.)
5. Setting BR2_REPRODUCIBLE weirdly made things worse. ????
