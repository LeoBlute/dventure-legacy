#define CARRAY(Type, Size) struct { \
   Type Data[Size];\
   u64 Commited; \
}

#define ARRAY_LENGTH(Array) (sizeof(Array) / sizeof(Array[0]))

#define CARRAY_ZERO(CArray) { \
   char* CArrayBytes = CArray.Data; \
   for(u64 ByteIndex = 0; ByteIndex < sizeof(CArray.Data); ++ByteIndex) { \
      CArrayBytes[ByteIndex] = 0; \
   } \
   CArray.Commited = 0; \
}

#define CARRAY_INSERT(CArray, Item) { \
   assert(CArray.Commited < ARRAY_LENGTH(CArray.Data)); \
   CArray.Data[CArray.Commited] = Item; \
   CArray.Commited++; \
} 

#define CARRAY_FOR_EACH(Item, CArray) for (typeof((CArray.Data[0])) *p = (CArray.Data), Item = *p; \
                                           p < &(CArray.Data[CArray.Commited]); \
                                           Item = *p++)

typedef struct file_data {
   u64 ChangedID;
   u64 ContentSize;
} file_data;

typedef struct _file_data {
   u64 modified_id;
   u64 content_size;
   char* name;
   char* content;
} _file_data;

#define FILE_DATA_CHANGED(FD1, FD2) (FD1.ChangedID != FD2.ChangedID || FD1.ContentSize != FD2.ContentSize)
