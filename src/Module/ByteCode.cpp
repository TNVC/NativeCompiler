#include "MachineCode/ByteCode.h"

#include <cstdio>
#include <cstring>
#include <malloc.h>
#include <cassert>

namespace db
{

  static bool CheckFileType(FILE *file, size_t *cmdCount);

  ByteCode *GetByteCode(const char *filePath)
  {
    assert(filePath);

    FILE *file = fopen(filePath, "rb");
    if (!file)
      {
        fprintf(stderr,
                "Fail to open \"%s\". File: \"%s\", Line: %d.\n",
                filePath, __FILE__, __LINE__);
        return nullptr;
      }

    size_t cmdCount = 0;
    if (!CheckFileType(file, &cmdCount)) { fclose(file); return nullptr; }

    ByteCode *code =
        (ByteCode *) calloc(1, sizeof(ByteCode));
    if (!code)
     {
       fprintf(stderr,
               "Out of memory. File: \"%s\", Line: %d.\n",
               __FILE__, __LINE__);
       fclose(file);
       return nullptr;
     }

    code->size = cmdCount;
    code->data =
        (Cmd *) calloc(cmdCount, sizeof(Cmd));
    if (!code->data)
     {
       fprintf(stderr,
               "Out of memory. File: \"%s\", Line: %d.\n",
               __FILE__, __LINE__);
       free(code);
       fclose(file);
       return nullptr;
     }

    size_t index = 0;
    Cmd cmd{};
    while (fread(&cmd.header, sizeof(CmdHeader), 1, file))
      {
        #define ARGS(TYPE) sizeof(TYPE), 1, file

        bool hasReadArg = true;
        if (cmd.header.immed)
          hasReadArg =
              fread(&cmd.data, ARGS(data_t));
        if (hasReadArg && cmd.header.reg)
          hasReadArg =
              fread(&cmd.reg , ARGS(cmd_t ));

        #undef ARGS

        if (!hasReadArg)
         {
           fprintf(stderr,
                   "Corrupted file. File: \"%s\", Line: %d.\n",
                   __FILE__, __LINE__);
           free(code->data);
           free(code);
           fclose(file);
           return nullptr;
         }

        code->data[index++] = cmd;
        if (index == cmdCount) break;
      }

    fclose(file);
    return code;
  }

  void DestroyByteCode(ByteCode *code)
  {
    assert(code);

    free(code->data);
    free(code);
  }

  static bool CheckFileType(FILE *file, size_t *cmdCount)
  {
    static const char SECURITY_CODE[] = "DB";
    static const char SOFTCPU_CMD_VERSION = 2;

    assert(file && cmdCount);

    struct {
      char securityCode[3];
      char version;
      char videoMode;
      int cmdCount;
    } title {};
    fread(&title, sizeof(title), 1, file);

    if (strcmp(title.securityCode, SECURITY_CODE))
     {
       fprintf(stderr,
                "Invalid file type. File: \"%s\", Line: %d.\n",
                __FILE__, __LINE__);
       return false;
     }
    if (title.version != SOFTCPU_CMD_VERSION)
     {
       fprintf(stderr,
                "Incorrect CPU version: %d. Expected: %d. File: \"%s\", Line: %d.\n",
                title.version, SOFTCPU_CMD_VERSION, __FILE__, __LINE__);
       return false;
     }
    if (title.cmdCount <= 0)
     {
       fprintf(stderr,
                "Corrupted file. File: \"%s\", Line: %d.\n",
                __FILE__, __LINE__);
       return false;
     }

    *cmdCount = title.cmdCount;
    return true;
  }

}