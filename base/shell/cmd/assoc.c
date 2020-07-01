/*
 *  ASSOC.C - assoc internal command.
 *
 *
 *  History:
 *
 * 14-Mar-2009 Lee C. Baker
 * - initial implementation.
 *
 * 15-Mar-2009 Lee C. Baker
 * - Don't write to (or use) HKEY_CLASSES_ROOT directly.
 * - Externalize strings.
 *
 * TODO:
 * - PrintAllAssociations could be optimized to not fetch all registry subkeys under 'Classes', just the ones that start with '.'
 * - Make sure that non-administrator users can list associations, and get appropriate error messages when they don't have sufficient
 *   privileges to perform an operation.
 */

#include "precomp.h"

#ifdef INCLUDE_CMD_ASSOC

static INT
PrintAssociation(
    IN LPCTSTR extension)
{
    DWORD lRet;
    HKEY hKey = NULL, hSubKey = NULL;
    DWORD fileTypeLength = 0;
    LPTSTR fileType = NULL;

    lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Classes"), 0, KEY_READ, &hKey);
    if (lRet != ERROR_SUCCESS)
        return -1;

    lRet = RegOpenKeyEx(hKey, extension, 0, KEY_READ, &hSubKey);
    RegCloseKey(hKey);

    if (lRet != ERROR_SUCCESS)
        return 0;

    /* Obtain string length */
    lRet = RegQueryValueEx(hSubKey, NULL, NULL, NULL, NULL, &fileTypeLength);

    /* If there is no default value, don't display */
    if (lRet == ERROR_FILE_NOT_FOUND)
    {
        RegCloseKey(hSubKey);
        return 0;
    }
    if (lRet != ERROR_SUCCESS)
    {
        RegCloseKey(hSubKey);
        return -2;
    }

    fileType = cmd_alloc(fileTypeLength * sizeof(TCHAR));
    if (!fileType)
    {
        WARN("Cannot allocate memory for fileType!\n");
        RegCloseKey(hSubKey);
        return -2;
    }

    /* Obtain actual file type */
    lRet = RegQueryValueEx(hSubKey, NULL, NULL, NULL, (LPBYTE)fileType, &fileTypeLength);
    RegCloseKey(hSubKey);

    if (lRet != ERROR_SUCCESS)
    {
        cmd_free(fileType);
        return -2;
    }

    /* If there is a default key, display relevant information */
    if (fileTypeLength != 0)
    {
        ConOutPrintf(_T("%s=%s\n"), extension, fileType);
    }

    cmd_free(fileType);
    return 1;
}

static INT
PrintAllAssociations(VOID)
{
    DWORD lRet = 0;
    HKEY hKey = NULL;
    DWORD numKeys = 0;

    DWORD extLength = 0;
    LPTSTR extName = NULL;
    DWORD keyCtr = 0;

    lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Classes"), 0, KEY_READ, &hKey);
    if (lRet != ERROR_SUCCESS)
        return -1;

    lRet = RegQueryInfoKey(hKey, NULL, NULL, NULL, &numKeys, &extLength,
                           NULL, NULL, NULL, NULL, NULL, NULL);
    if (lRet != ERROR_SUCCESS)
    {
        RegCloseKey(hKey);
        return -2;
    }

    extLength++;
    extName = cmd_alloc(extLength * sizeof(TCHAR));
    if (!extName)
    {
        WARN("Cannot allocate memory for extName!\n");
        RegCloseKey(hKey);
        return -2;
    }

    for (keyCtr = 0; keyCtr < numKeys; ++keyCtr)
    {
        DWORD dwBufSize = extLength;
        lRet = RegEnumKeyEx(hKey, keyCtr, extName, &dwBufSize,
                            NULL, NULL, NULL, NULL);

        if (lRet == ERROR_SUCCESS || lRet == ERROR_MORE_DATA)
        {
            if (*extName == _T('.'))
                PrintAssociation(extName);
        }
        else
        {
            cmd_free(extName);
            RegCloseKey(hKey);
            return -1;
        }
    }

    RegCloseKey(hKey);

    cmd_free(extName);
    return numKeys;
}

static INT
AddAssociation(
    IN LPCTSTR extension,
    IN LPCTSTR type)
{
    DWORD lRet;
    HKEY hKey = NULL, hSubKey = NULL;

    lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Classes"), 0, KEY_ALL_ACCESS, &hKey);
    if (lRet != ERROR_SUCCESS)
        return -1;

    lRet = RegCreateKeyEx(hKey, extension, 0, NULL, REG_OPTION_NON_VOLATILE,
                          KEY_ALL_ACCESS, NULL, &hSubKey, NULL);
    RegCloseKey(hKey);

    if (lRet != ERROR_SUCCESS)
        return -1;

    lRet = RegSetValueEx(hSubKey, NULL, 0, REG_SZ,
                         (LPBYTE)type, (_tcslen(type) + 1) * sizeof(TCHAR));
    RegCloseKey(hSubKey);

    if (lRet != ERROR_SUCCESS)
        return -2;

    return 0;
}

static INT
RemoveAssociation(
    IN LPCTSTR extension)
{
    DWORD lRet;
    HKEY hKey;

    lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Classes"), 0, KEY_ALL_ACCESS, &hKey);
    if (lRet != ERROR_SUCCESS)
        return -1;

    lRet = RegDeleteKey(hKey, extension);
    RegCloseKey(hKey);

    if (lRet != ERROR_SUCCESS)
        return -2;

    return 0;
}


INT CommandAssoc(LPTSTR param)
{
    INT retval = 0;
    LPTSTR lpEqualSign;

    /* Print help */
    if (!_tcsncmp(param, _T("/?"), 2))
    {
        ConOutResPaging(TRUE, STRING_ASSOC_HELP);
        return 0;
    }

    if (_tcslen(param) == 0)
    {
        PrintAllAssociations();
        goto Quit;
    }

    lpEqualSign = _tcschr(param, _T('='));
    if (lpEqualSign != NULL)
    {
        LPTSTR fileType = lpEqualSign + 1;
        LPTSTR extension = cmd_alloc((lpEqualSign - param + 1) * sizeof(TCHAR));
        if (!extension)
        {
            WARN("Cannot allocate memory for extension!\n");
            error_out_of_memory();
            retval = 1;
            goto Quit;
        }

        _tcsncpy(extension, param, lpEqualSign - param);
        extension[lpEqualSign - param] = _T('\0');

        /* If the equal sign is the last character
         * in the string, then delete the key. */
        if (_tcslen(fileType) == 0)
        {
            retval = RemoveAssociation(extension);
        }
        else
        /* Otherwise, add the key and print out the association */
        {
            retval = AddAssociation(extension, fileType);
            PrintAssociation(extension);
        }

        cmd_free(extension);

        if (retval)
            retval = 1; /* Fixup the error value */
    }
    else
    {
        /* No equal sign, print all associations */
        retval = PrintAssociation(param);
        if (retval == 0)    /* If nothing printed out */
        {
            ConOutResPrintf(STRING_ASSOC_ERROR, param);
            retval = 1; /* Fixup the error value */
        }
    }

Quit:
    if (BatType != CMD_TYPE)
    {
        if (retval != 0)
            nErrorLevel = retval;
    }
    else
    {
        nErrorLevel = retval;
    }

    return retval;
}

#endif /* INCLUDE_CMD_ASSOC */
