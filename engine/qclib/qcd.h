pbool QC_decodeMethodSupported(int method);
char *QC_decode(progfuncs_t *progfuncs, int complen, int len, int method, const char *info, char *buffer);
int QC_encode(progfuncs_t *progfuncs, int len, int method, const char *in, int handle);
int QC_EnumerateFilesFromBlob(const void *blob, size_t blobsize, void (*cb)(const char *name, const void *compdata, size_t compsize, int method, size_t plainsize));
int QC_encodecrc(int len, char *in);

char *PDECL filefromprogs(pubprogfuncs_t *progfuncs, progsnum_t prnum, char *fname, size_t *size, char *buffer);
char *filefromnewprogs(pubprogfuncs_t *progfuncs, char *prname, char *fname, size_t *size, char *buffer);//fixme - remove parm 1

void DecompileProgsDat(char *name, void *buf, size_t bufsize);
