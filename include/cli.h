#ifndef CLI_H
#define CLI_H
int cli_help(void);
int cli_version(void);
/* files : liste de fichiers à analyser. */
int cmd_scan(int n, char **files);
/* args : <fichier> [rapport.md] */
int cmd_report(int n, char **args);
#endif