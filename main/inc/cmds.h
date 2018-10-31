/*
 * cmds.h
 *
 *  Created on: Apr 16, 2017
 *      Author: RSN
 */

#ifndef MAIN_CMDS_H_
#define MAIN_CMDS_H_
extern void kbd(void *pArg);
extern void displayManager(void *pArg);
extern void erase_config();
extern void drawString(int x, int y, string que, int fsize, int align,displayType showit,overType erase);
extern void setLogo(string cual);
#endif /* MAIN_CMDS_H_ */
