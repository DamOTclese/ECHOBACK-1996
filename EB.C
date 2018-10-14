
/* **********************************************************************
   * EB.C - Echo Back                                                   *
   *                                                                    *
   * Copyright by Fredric L. Rice, January 1996.                        *
   *                                                                    *
   * The Skeptic Tank, 1:102/890.0, (818) 335-9601.                     *
   *                                                                    *
   ********************************************************************** */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alloc.h>
#include <conio.h>
#include <dir.h>
#include <time.h>
#include <ctype.h>
#include <time.h>

/* **********************************************************************
   * Any macros?                                                        *
   *                                                                    *
   ********************************************************************** */

#define The_Version     "1.00"
#define TRUE            1
#define FALSE           0
#define UCHAR           unsigned char
#define BOOL            unsigned char
#define ULONG           unsigned long
#define USHORT          unsigned short
#define skipspace(s)    while (isspace(*s))     ++s

#define AREA_NAME_LEN   (50 + 1)

/* **********************************************************************
   * The message file format offered here is Fido format which has      *
   * been tested with OPUS and Dutchie. It represents the latest        *
   * format that I know about.  The format, in fact, appears to be      *
   * locked-up in the past, prompting the need for kludge-lines to      *
   * handle route information not included in the mindset of the early  *
   * pioneers of FidoNet.                                               *
   *                                                                    *
   ********************************************************************** */

    static struct fido_msg
    {
        char from[36];             /* Who the message is from             */
        char to[36];               /* Who the message to to               */
        char subject[72];          /* The subject of the message          */
        char date[20];             /* Message creation date/time          */
        USHORT times;              /* Number of time the message was read */
        USHORT destination_node;   /* Intended destination node           */
        USHORT originate_node;     /* The originator node of the message  */
        USHORT cost;               /* Cost to send this message           */
        USHORT originate_net;      /* The originator net of the message   */
        USHORT destination_net;    /* Intended destination net number     */
        USHORT destination_zone;   /* Intended zone for the message       */
        USHORT originate_zone;     /* The zone of the originating system  */
        USHORT destination_point;  /* Is there a point to destination?    */
        USHORT originate_point;    /* Point that originated the message   */
        USHORT reply;              /* Thread to previous reply            */
        USHORT attribute;          /* Message type                        */
        USHORT upwards_reply;      /* Thread to next message reply        */
    } message;                     /* Create one of this structure.       */

/* **********************************************************************
   * 'Attribute' bit definitions, some of which we will use             *
   *                                                                    *
   ********************************************************************** */

#define Fido_Private            0x0001
#define Fido_Crash              0x0002
#define Fido_Read               0x0004
#define Fido_Sent               0x0008
#define Fido_File_Attach        0x0010
#define Fido_Forward            0x0020
#define Fido_Orphan             0x0040
#define Fido_Kill               0x0080
#define Fido_Local              0x0100
#define Fido_Hold               0x0200
#define Fido_Reserved1          0x0400
#define Fido_File_Request       0x0800
#define Fido_Ret_Rec_Req        0x1000
#define Fido_Ret_Rec            0x2000
#define Fido_Req_Audit_Trail    0x4000
#define Fido_Update_Req         0x8000

/* **********************************************************************
   * Here are the errorlevels we are allowed to exit with.              *
   *                                                                    *
   ********************************************************************** */

#define No_Problem                      0
#define Cant_Find_Config_File           10
#define Configuration_Bad               11
#define No_Memory                       12
#define Cant_Create_History_File        13
#define Out_Of_Disk_Space               14
#define History_File_Corrupted          15
#define Cant_Create_Archive             16

/* **********************************************************************
   * Here is a linked list of the echo areas we're interested in.       *
   *                                                                    *
   ********************************************************************** */

    static struct Echo_Area
    {
        UCHAR  tag_name[AREA_NAME_LEN + 1];     /* Tag name of echo area  */
        UCHAR  echo_directory[101];             /* Path to the echo       */
        UCHAR  file_name[101];                  /* Path/File name archive */
        USHORT highest_archive;                 /* Number highest achive  */
        ULONG  archive_size;                    /* The size of archive    */
        struct Echo_Area *next;                 /* Pointer to the next    */
    } *ea_first, *ea_last, *ea_point;           /* Make three pointers    */

/* **********************************************************************
   * Define local data storage                                          *
   *                                                                    *
   ********************************************************************** */

    static UCHAR config_directory[101];
    static UCHAR update_month;
    static UCHAR update_day;
    static UCHAR update_year;
    static USHORT maximum_archive_size;
    static USHORT maximum_message_history;
    static ULONG *msgid;

/* **********************************************************************
   * search_for_extention()                                             *
   *                                                                    *
   ********************************************************************** */

static USHORT search_for_extention(UCHAR *fname)
{
    UCHAR *atpoint;

    atpoint = strchr(fname, '.');

    if (atpoint == (char *)NULL)
    {
        return((USHORT)0);
    }

    atpoint++;
    return((USHORT)atoi(atpoint));
}

/* **********************************************************************
   * find_highest_message_number()                                      *
   *                                                                    *
   * Find the highest archive file and return the numeric.              *
   *                                                                    *
   * Input:     UCHAR *archive                                          *
   *            Pointer to archile path and file name                   *
   *                                                                    *
   * Output:    void                                                    *
   *                                                                    *
   ********************************************************************** */

static USHORT find_highest_message_number(UCHAR *archive, ULONG *archive_size)
{
    UCHAR result;
    UCHAR directory_search[101];
    struct ffblk file_block;
    USHORT highest_message_number;
    USHORT test_number;

/*
 * Start out saying we don't have any
 */

    highest_message_number = 0;

    (void)strcpy(directory_search, archive);
    (void)strcat(directory_search, ".*");

/*
 * See if we have at least one
 */

    result = findfirst(directory_search, &file_block, 0x16);

    if (! result)
    {
        test_number = search_for_extention(file_block.ff_name);

        if (test_number > highest_message_number)
        {
            highest_message_number = test_number;
            *archive_size = file_block.ff_fsize;
        }
    }

/*
 * Scan all messages until we know the highest message number
 */

    while (! result)
    {
        result = findnext(&file_block);

        if (! result)
        {
            test_number = search_for_extention(file_block.ff_name);

            if (test_number > highest_message_number)
            {
                highest_message_number = test_number;
                *archive_size = file_block.ff_fsize;
            }
        }
    }

    return(highest_message_number);
}

/* **********************************************************************
   * plug_echo()                                                        *
   *                                                                    *
   * Plug the echo tag name and Snoop data File name.                   *
   *                                                                    *
   * Input:     char *atpoint                                           *
   *            Pointer to the rest of the configuration string.        *
   *                                                                    *
   * Output:    void                                                    *
   *                                                                    *
   ********************************************************************** */

static void plug_echo(char *atpoint)
{
    char echo_name[101];
    char echo_directory[101];
    char file_name[101];
    char count, length;

/*
 * We must have a valid echo tag
 */

    if (strlen(atpoint) < 1)
    {
        return;
    }

/*
 * Extract echo name
 */

    count = 0;

    while (*atpoint && *atpoint != ' ')
    {
        echo_name[count++] = *atpoint++;
    }

    echo_name[count] = (char)NULL;
    length = count;

    if (! *atpoint)
    {
        return;
    }

/*
 * Get echo directory
 */

    skipspace(atpoint);

    count = 0;

    while (*atpoint && *atpoint != ' ')
    {
        echo_directory[count++] = *atpoint++;
    }

    echo_directory[count] = (char)NULL;

/*
 * Append \ if path/file name doesn't have one
 */

    if (echo_directory[strlen(echo_directory) - 1] != '\\')
        (void)strcat(echo_directory, "\\");

/*
 * Get file name
 */

    skipspace(atpoint);

    count = 0;

    while (*atpoint && *atpoint != ' ')
    {
        file_name[count++] = *atpoint++;
    }

    file_name[count] = (char)NULL;

/*
 * Make sure the ECHO isn't already logged
 */

    ea_point = ea_first;

    while (ea_point)
    {
        if (! strnicmp(ea_point->tag_name, echo_name, length))
        {
            if (strlen(ea_point->tag_name) == length)
            {
                (void)printf("WARNING: ECHO tag %s is a duplicate\n",
                    echo_name);

                return;
            }
        }

        ea_point = ea_point->next;
    }

/*
 * Allocate a new entry
 */

    ea_point = (struct Echo_Area *)farmalloc(sizeof(struct Echo_Area));

    if (ea_point == (struct Echo_Area *)NULL)
    {
        (void)printf("ERROR: I ran out of memory!\n");
        exit(No_Memory);
    }

/*
 * Initialize the elements in the structure
 */

    for (count = 0; count < AREA_NAME_LEN; count++)
        ea_point->tag_name[count] = (UCHAR)NULL;

    (void)strcpy(ea_point->tag_name, echo_name);
    (void)strcpy(ea_point->echo_directory, echo_directory);
    (void)strcpy(ea_point->file_name, file_name);

    ea_point->highest_archive =
        find_highest_message_number(file_name, &ea_point->archive_size);

/*
 * Put it into the linked list
 */

    if (ea_first == (struct Echo_Area *)NULL)
    {
        ea_first = ea_point;
    }
    else
    {
        ea_last->next = ea_point;
    }

    ea_point->next = (struct Echo_Area *)NULL;
    ea_last = ea_point;
}

/* **********************************************************************
   * extract_configuration()                                            *
   *                                                                    *
   * Extract the configuration file information.                        *
   *                                                                    *
   * Input:     void                                                    *
   *                                                                    *
   * Output:    void                                                    *
   *                                                                    *
   ********************************************************************** */
   
static void extract_configuration(void)
{
    FILE *fin;
    char record[201], *atpoint;

/*
 * Open the file
 */

    if ((fin = fopen(config_directory, "rt")) == (FILE *)NULL)
    {
        (void)printf("Configuration file [%s] can't be found!\n",
            config_directory);

        fcloseall();
        exit(Cant_Find_Config_File);
    }

/*
 * Go through the file extracting all lines.
 * Then evaluate what they are.
 */

    while (! feof(fin))
    {
        (void)fgets(record, 200, fin);

        if (! feof(fin))
        {
            atpoint = record;
            skipspace(atpoint);
            atpoint[strlen(atpoint) - 1] = (char)NULL;

            if (! strnicmp(atpoint, "echo", 4))
            {
                atpoint += 4;
                skipspace(atpoint);
                plug_echo(atpoint);
            }
            else if (! strnicmp(atpoint, "maxsize", 7))
            {
                atpoint += 7;
                skipspace(atpoint);
                maximum_archive_size = atoi(atpoint);
            }
            else if (! strnicmp(atpoint, "history", 7))
            {
                atpoint += 7;
                skipspace(atpoint);
                maximum_message_history = atoi(atpoint);
            }
        }
    }

/*
 * Close the file
 */

    (void)fclose(fin);
}

/* **********************************************************************
   * validate_configuration()                                           *
   *                                                                    *
   * Make sure that we got all of the configuration we need.            *
   *                                                                    *
   * Input:     void                                                    *
   *                                                                    *
   * Output:    TRUE if configuration looks good                        *
   *            FALSE if something in configuration looks bad           *
   *                                                                    *
   ********************************************************************** */

static BOOL validate_configuration(void)
{

/*
 * Make sure at least one echo command was offered
 */

    if (ea_first == (struct Echo_Area *)NULL)
    {
        (void)printf("FATAL: Config file hasn't any 'echo' commands.\n");
        return(FALSE);
    }

/*
 * If it all passed, return TRUE
 */

    return(TRUE);
}

/* **********************************************************************
   * initialize()                                                       *
   *                                                                    *
   * Initialize this.                                                   *
   *                                                                    *
   * Input:     void                                                    *
   *                                                                    *
   * Output:    void                                                    *
   *                                                                    *
   ********************************************************************** */
   
static void initialize(void)
{
    UCHAR  *env;
    USHORT loop;
    time_t t_current;
    struct tm *green_time;

/*
 * Get our environment variable to determine the path
 * to our configuration file and error message file
 */

    if (NULL == (env = getenv("EB")))
    {
        (void)strcpy(config_directory, "EB.CFG");
    }
    else
    {
        (void)strcpy(config_directory, env);

        if (config_directory[strlen(config_directory) - 1] != '\\')
        {
            (void)strcat(config_directory, "\\");
        }

        (void)strcat(config_directory, "EB.CFG");
    }

/*
 * Initialize local data storage
 */

    ea_first = ea_last = ea_point = (struct Echo_Area *)NULL;
    maximum_archive_size = (USHORT)0;
    maximum_message_history = 10000;

/*
 * Get the date into our update variables
 */

    (void)time(&t_current);
    green_time = gmtime(&t_current);

    update_month = (UCHAR)green_time->tm_mon;
    update_day = (UCHAR)green_time->tm_mday;
    update_year = (UCHAR)green_time->tm_year;

/*
 * Allocate space for the MSGID's
 */

    msgid = farmalloc((ULONG)sizeof(ULONG) * (ULONG)maximum_message_history);

    if (msgid == (ULONG *)NULL)
    {
        (void)printf("I ran out of memory while trying to allocate space\n");
        (void)printf("to store all the Echo Mail's MSGID values.\n");
        (void)printf("Reduce the maximum number of messages checked\n");
        (void)printf("for history in the configuration file.\n");
        fcloseall();
        exit(No_Memory);
    }
}

/* **********************************************************************
   * center()                                                           *
   *                                                                    *
   * Center the string on the screen                                    *
   *                                                                    *
   * Input:     char *this                                              *
   *            Pointer to the character string we'll be printing.      *
   *                                                                    *
   * Output:    void                                                    *
   *                                                                    *
   ********************************************************************** */

static void center(char *this)
{
    int length;

    length = (76 - strlen(this)) / 2;

    while (length > 0)
    {
        (void)putchar(' ');
        length--;
    }

    (void)printf(this);
}

/* **********************************************************************
   * say_hello()                                                        *
   *                                                                    *
   * Say hello.                                                         *
   *                                                                    *
   * Input:     void                                                    *
   *                                                                    *
   * Output:    void                                                    *
   *                                                                    *
   ********************************************************************** */

static void say_hello(void)
{
    UCHAR buffer[51];

    clrscr();
    (void)printf("\n");

    (void)sprintf(buffer, "Echo Back Version %s (%s)\n\n",
        The_Version, __DATE__);

    center(buffer);
}

/* **********************************************************************
   * read_history_file()                                                *
   *                                                                    *
   ********************************************************************** */

static void read_or_create_history_file(struct Echo_Area *ea_this)
{
    UCHAR msgid_file[101];
    USHORT result;
    FILE *fin;

/*
 * Build a signature file path name
 */

    (void)strcpy(msgid_file, ea_this->file_name);
    (void)strcat(msgid_file, ".HIS");

/*
 * See if the file exists.  If it does, read it in.  If it
 * does not, create it then write out an empty file.
 */

   if ((fin = fopen(msgid_file, "rb")) == (FILE *)NULL)
   {
        if ((fin = fopen(msgid_file, "wb")) == (FILE *)NULL)
        {
            (void)printf("I can't create file: %s\n", msgid_file);
            fcloseall();
            exit(Cant_Create_History_File);
        }

        result =
            fwrite(msgid, sizeof(ULONG), maximum_message_history + 1, fin);

        if (result != maximum_message_history + 1)
        {
            (void)printf("I appear to have run out of disk space!\n");
            (void)printf("I only created %d MSGID records\n", result);
            fcloseall();
            exit(Out_Of_Disk_Space);
        }
    }
    else
    {
        result =
            fread(msgid, sizeof(ULONG), maximum_message_history + 1, fin);

        if (result != maximum_message_history + 1)
        {
            (void)printf("I appear to have a corrupted history file: %s\n",
                msgid_file);

            (void)printf("Is it possible you changed the 'history' value\n");
            (void)printf("in your configuration file to a value larger\n");
            (void)printf("then it was when the history file was created?\n");

            fcloseall();
            exit(History_File_Corrupted);
        }
    }

    (void)fclose(fin);
}

/* **********************************************************************
   * look_for_msgid()                                                   *
   *                                                                    *
   ********************************************************************** */

static BOOL look_for_msgid(ULONG msgid_value)
{
    USHORT loop;

    for (loop = 1; loop < maximum_message_history; loop++)
    {
        if (msgid[loop] == msgid_value)
        {
            return(TRUE);
        }
    }

    return(FALSE);
}

/* **********************************************************************
   * archive_this_message()                                             *
   *                                                                    *
   ********************************************************************** */

static void archive_this_message(struct Echo_Area *ea_this, FILE *fin)
{
    FILE *fout;
    USHORT result;
    UCHAR file_name[101];
    UCHAR record[101], *atpoint, byte;
    struct ffblk file_block;

/*
 * Rewind the message
 */

    rewind(fin);

/*
 * Get the header again
 */

    result = fread(&message, sizeof(struct fido_msg), 1, fin);

/*
 * Build the archive file name
 */

    (void)sprintf(file_name, "%s.%03d",
        ea_this->file_name,
        ea_this->highest_archive);

/*
 * Open the archive file for append
 */

    if ((fout = fopen(file_name, "a+b")) == (FILE *)NULL)
    {
        (void)printf("I can't open/create file: [%s]!\n", file_name);
        fcloseall();
        exit(Cant_Create_Archive);
    }

    (void)sprintf(record, "|From: %s\n", message.from);
    (void)fputs(record, fout);
    (void)sprintf(record, "|To:   %s\n", message.to);
    (void)fputs(record, fout);
    (void)sprintf(record, "|Sub:  %s\n", message.subject);
    (void)fputs(record, fout);
    (void)sprintf(record, "|Date: %s\n", message.date);
    (void)fputs(record, fout);

    while (! feof(fin))
    {
        (void)fgets(record, 100, fin);

        atpoint = record;

        while (*atpoint)
        {
            byte = *atpoint;

            if (byte != 0x0a && byte != 0x01)
            {
                (void)fputc(byte, fout);
            }

            if (byte == 0x0d)
            {
                (void)fputc(0x0a, fout);
            }

            atpoint++;
        }
    }

/*
 * Close the output archive file
 */

    (void)fputs("\n", fout);
    (void)fclose(fout);

/*
 * Get the size of the archive file
 */

    result = findfirst(file_name, &file_block, 0x16);

    if (! result)
    {
        if (file_block.ff_fsize >= (ULONG)maximum_archive_size * 1000L)
        {
            ea_this->highest_archive++;
            ea_this->archive_size = file_block.ff_fsize;
        }
    }
}

/* **********************************************************************
   * examine_this_message()                                             *
   *                                                                    *
   ********************************************************************** */

static void examine_this_message(struct Echo_Area *ea_this, UCHAR *fname)
{
    FILE *fin;
    UCHAR directory_search[101];
    UCHAR record[101], *atpoint;
    USHORT result;
    UCHAR msgid_search_string[10];
    UCHAR eid_search_string[10];
    ULONG msgid_value;
    ULONG index;
    USHORT eid_value;

/*
 * Build the path and file name
 */

    (void)strcpy(directory_search, ea_this->echo_directory);
    (void)strcat(directory_search, fname);

/*
 * Open the file
 */

    if ((fin = fopen(directory_search, "rb")) == (FILE *)NULL)
    {
        (void)printf("WARNING: I couldn't open: %s\n", directory_search);
        return;
    }

/*
 * Read the header
 */

    result = fread(&message, sizeof(struct fido_msg), 1, fin);

    if (result != 1)
    {
        (void)printf("WARNING: I couldn't read: %s\n", directory_search);
        (void)fclose(fin);
        return;
    }

    (void)printf("[%s]\r    ", directory_search);
    (void)sprintf(msgid_search_string, "%cMSGID:", 0x01);
    (void)sprintf(eid_search_string, "%cEID:", 0x01);

    eid_value = 1;

/*
 * Read the file until MSGID is found
 */

    while (! feof(fin))
    {
        (void)fgets(record, 100, fin);

        if (! feof(fin))
        {
            atpoint = strstr(record, msgid_search_string);

            if (atpoint == (UCHAR *)NULL)
            {
                atpoint = strstr(record, eid_search_string);

                if (atpoint != (UCHAR *)NULL)
                {
                    atpoint += 5;

                    (void)sscanf(atpoint, "%x", &eid_value);

                    while (*atpoint != ' ')
                    {
                        atpoint++;
                    }

                    skipspace(atpoint);
                    atpoint[8] = (UCHAR)NULL;
                }
            }
            else
            {
                atpoint += 7;
                skipspace(atpoint);

                while (*atpoint != ' ')
                {
                    atpoint++;
                }

                skipspace(atpoint);
                atpoint[8] = (UCHAR)NULL;
            }

            if (atpoint != (UCHAR *)NULL)
            {
                (void)sscanf(atpoint, "%lx", &msgid_value);

                msgid_value *= eid_value;

                if (! look_for_msgid(msgid_value))
                {
                    index = msgid[0] + 1L;

                    if ((USHORT)index >= maximum_message_history)
                    {
                        index = 1L;
                    }

                    msgid[0] = index;
                    msgid[index] = msgid_value;

                    archive_this_message(ea_this, fin);
                }
            }
        }
    }

/*
 * Close the MSG file
 */

    (void)fclose(fin);
}

/* **********************************************************************
   * examine_echo_mail()                                                *
   *                                                                    *
   * Input:     ea_this                                                 *
   *            Pointer to the data structure which describes the       *
   *            echo mail area we're working with.                      *
   *                                                                    *
   * Output:    void                                                    *
   *                                                                    *
   ********************************************************************** */

static void examine_echo_mail(struct Echo_Area *ea_this)
{
    UCHAR search_directory[101];
    USHORT result;
    struct ffblk file_block;

/*
 * Build a signature file path name
 */

    (void)strcpy(search_directory, ea_this->echo_directory);
    (void)strcat(search_directory, "*.MSG");

    (void)printf("Searching [%s]\n", search_directory);

/*
 * See if we have a first one
 */

    result = findfirst(search_directory, &file_block, 0x16);

    if (! result)
    {
        examine_this_message(ea_this, file_block.ff_name);
    }

/*
 * Scan all messages until we know the highest message number
 */

    while (! result)
    {
        result = findnext(&file_block);

        if (! result)
        {
            examine_this_message(ea_this, file_block.ff_name);
        }
    }
}

/* **********************************************************************
   * update_history_file()                                              *
   *                                                                    *
   * Input:     ea_this                                                 *
   *            Pointer to the data structure which describes the       *
   *            echo mail area we're working with.                      *
   *                                                                    *
   * Output:    void                                                    *
   *                                                                    *
   ********************************************************************** */

static void update_history_file(struct Echo_Area *ea_this)
{
    UCHAR msgid_file[101];
    USHORT result;
    FILE *fout;

/*
 * Build a signature file path name
 */

    (void)strcpy(msgid_file, ea_this->file_name);
    (void)strcat(msgid_file, ".HIS");

/*
 * Create the history file, overwrite the existing one
 */

    if ((fout = fopen(msgid_file, "wb")) == (FILE *)NULL)
    {
        (void)printf("I can't create file: %s\n", msgid_file);
        fcloseall();
        exit(Cant_Create_History_File);
    }

    result = fwrite(msgid, sizeof(ULONG), maximum_message_history + 1, fout);

    if (result != maximum_message_history + 1)
    {
        (void)printf("I appear to have run out of disk space!\n");
        (void)printf("I only created %d MSGID records\n", result);
        fcloseall();
        exit(Out_Of_Disk_Space);
    }

/*
 * We're done so close the history file
 */

    (void)fclose(fout);
}

/* **********************************************************************
   * process_this_echo_directory()                                      *
   *                                                                    *
   * Input:     ea_this                                                 *
   *            Pointer to the data structure which describes the       *
   *            echo mail area we're working with.                      *
   *                                                                    *
   * Output:    void                                                    *
   *                                                                    *
   ********************************************************************** */

static void process_this_echo_directory(struct Echo_Area *ea_this)
{
    USHORT loop;

/*
 * Zero the memory allocated
 */

    for (loop = 0; loop < maximum_message_history; loop++)
    {
        msgid[loop] = 0L;
    }

    read_or_create_history_file(ea_this);
    examine_echo_mail(ea_this);
    update_history_file(ea_this);
}

/* **********************************************************************
   * process_echo_mail()                                                *
   *                                                                    *
   * Go through the linked list and process each echo directory.        *
   *                                                                    *
   * Input:     void                                                    *
   *                                                                    *
   * Output:    void                                                    *
   *                                                                    *
   ********************************************************************** */

static void process_echo_mail(void)
{
    ea_point = ea_first;

    while (ea_point)
    {
        process_this_echo_directory(ea_point);
        ea_point = ea_point->next;
    }
}

/* **********************************************************************
   * main()                                                             *
   *                                                                    *
   * Main entry point.                                                  *
   *                                                                    *
   * Input:     USHORT argc                                             *
   *            Contains the number of arguments passed to us           *
   *                                                                    *
   *            USHORT *argv[]                                          *
   *            Pointer to an array of pointers to the arguments        *
   *                                                                    *
   * Output:    ERRORLEVEL                                              *
   *                                                                    *
   ********************************************************************** */
   
void main(void)
{
/*
 * Initialize us
 */

    initialize();

/*
 * Get our configuration
 */

    extract_configuration();

    if (! validate_configuration())
    {
        farfree(msgid);
        exit(Configuration_Bad);
    }

/*
 * Tell the world that we exist
 */

    say_hello();

/*
 * Process the directories
 */

    process_echo_mail();

/*
 * Close up shop
 */

    fcloseall();

/*
 * Terminate with a no-problem errorlevel value
 */

    farfree(msgid);
    exit(No_Problem);
}

