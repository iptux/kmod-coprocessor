/*
 * mcu-oled.c
 * mcu coprocessor bus protocol, oled driver
 *
 * Author: Alex.wang
 * Create: 2015-07-07 15:57
 */


#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/miscdevice.h>
#include <linux/mcu.h>
#include <linux/lq12864.h>
#include "mcu-internal.h"

static u8 F6x8[][6] =
{
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },	// sp
	{ 0x00, 0x00, 0x00, 0x2f, 0x00, 0x00 },	// !
	{ 0x00, 0x00, 0x07, 0x00, 0x07, 0x00 },	// "
	{ 0x00, 0x14, 0x7f, 0x14, 0x7f, 0x14 },	// #
	{ 0x00, 0x24, 0x2a, 0x7f, 0x2a, 0x12 },	// $
	{ 0x00, 0x62, 0x64, 0x08, 0x13, 0x23 },	// %
	{ 0x00, 0x36, 0x49, 0x55, 0x22, 0x50 },	// &
	{ 0x00, 0x00, 0x05, 0x03, 0x00, 0x00 },	// '
	{ 0x00, 0x00, 0x1c, 0x22, 0x41, 0x00 },	// (
	{ 0x00, 0x00, 0x41, 0x22, 0x1c, 0x00 },	// )
	{ 0x00, 0x14, 0x08, 0x3E, 0x08, 0x14 },	// *
	{ 0x00, 0x08, 0x08, 0x3E, 0x08, 0x08 },	// +
	{ 0x00, 0x00, 0x00, 0xA0, 0x60, 0x00 },	// ,
	{ 0x00, 0x08, 0x08, 0x08, 0x08, 0x08 },	// -
	{ 0x00, 0x00, 0x60, 0x60, 0x00, 0x00 },	// .
	{ 0x00, 0x20, 0x10, 0x08, 0x04, 0x02 },	// /
	{ 0x00, 0x3E, 0x51, 0x49, 0x45, 0x3E },	// 0
	{ 0x00, 0x00, 0x42, 0x7F, 0x40, 0x00 },	// 1
	{ 0x00, 0x42, 0x61, 0x51, 0x49, 0x46 },	// 2
	{ 0x00, 0x21, 0x41, 0x45, 0x4B, 0x31 },	// 3
	{ 0x00, 0x18, 0x14, 0x12, 0x7F, 0x10 },	// 4
	{ 0x00, 0x27, 0x45, 0x45, 0x45, 0x39 },	// 5
	{ 0x00, 0x3C, 0x4A, 0x49, 0x49, 0x30 },	// 6
	{ 0x00, 0x01, 0x71, 0x09, 0x05, 0x03 },	// 7
	{ 0x00, 0x36, 0x49, 0x49, 0x49, 0x36 },	// 8
	{ 0x00, 0x06, 0x49, 0x49, 0x29, 0x1E },	// 9
	{ 0x00, 0x00, 0x36, 0x36, 0x00, 0x00 },	// :
	{ 0x00, 0x00, 0x56, 0x36, 0x00, 0x00 },	// ;
	{ 0x00, 0x08, 0x14, 0x22, 0x41, 0x00 },	// <
	{ 0x00, 0x14, 0x14, 0x14, 0x14, 0x14 },	// =
	{ 0x00, 0x00, 0x41, 0x22, 0x14, 0x08 },	// >
	{ 0x00, 0x02, 0x01, 0x51, 0x09, 0x06 },	// ?
	{ 0x00, 0x32, 0x49, 0x59, 0x51, 0x3E },	// @
	{ 0x00, 0x7C, 0x12, 0x11, 0x12, 0x7C },	// A
	{ 0x00, 0x7F, 0x49, 0x49, 0x49, 0x36 },	// B
	{ 0x00, 0x3E, 0x41, 0x41, 0x41, 0x22 },	// C
	{ 0x00, 0x7F, 0x41, 0x41, 0x22, 0x1C },	// D
	{ 0x00, 0x7F, 0x49, 0x49, 0x49, 0x41 },	// E
	{ 0x00, 0x7F, 0x09, 0x09, 0x09, 0x01 },	// F
	{ 0x00, 0x3E, 0x41, 0x49, 0x49, 0x7A },	// G
	{ 0x00, 0x7F, 0x08, 0x08, 0x08, 0x7F },	// H
	{ 0x00, 0x00, 0x41, 0x7F, 0x41, 0x00 },	// I
	{ 0x00, 0x20, 0x40, 0x41, 0x3F, 0x01 },	// J
	{ 0x00, 0x7F, 0x08, 0x14, 0x22, 0x41 },	// K
	{ 0x00, 0x7F, 0x40, 0x40, 0x40, 0x40 },	// L
	{ 0x00, 0x7F, 0x02, 0x0C, 0x02, 0x7F },	// M
	{ 0x00, 0x7F, 0x04, 0x08, 0x10, 0x7F },	// N
	{ 0x00, 0x3E, 0x41, 0x41, 0x41, 0x3E },	// O
	{ 0x00, 0x7F, 0x09, 0x09, 0x09, 0x06 },	// P
	{ 0x00, 0x3E, 0x41, 0x51, 0x21, 0x5E },	// Q
	{ 0x00, 0x7F, 0x09, 0x19, 0x29, 0x46 },	// R
	{ 0x00, 0x46, 0x49, 0x49, 0x49, 0x31 },	// S
	{ 0x00, 0x01, 0x01, 0x7F, 0x01, 0x01 },	// T
	{ 0x00, 0x3F, 0x40, 0x40, 0x40, 0x3F },	// U
	{ 0x00, 0x1F, 0x20, 0x40, 0x20, 0x1F },	// V
	{ 0x00, 0x3F, 0x40, 0x38, 0x40, 0x3F },	// W
	{ 0x00, 0x63, 0x14, 0x08, 0x14, 0x63 },	// X
	{ 0x00, 0x07, 0x08, 0x70, 0x08, 0x07 },	// Y
	{ 0x00, 0x61, 0x51, 0x49, 0x45, 0x43 },	// Z
	{ 0x00, 0x00, 0x7F, 0x41, 0x41, 0x00 },	// [
	{ 0x00, 0x55, 0x2A, 0x55, 0x2A, 0x55 },	// 55
	{ 0x00, 0x00, 0x41, 0x41, 0x7F, 0x00 },	// ]
	{ 0x00, 0x04, 0x02, 0x01, 0x02, 0x04 },	// ^
	{ 0x00, 0x40, 0x40, 0x40, 0x40, 0x40 },	// _
	{ 0x00, 0x00, 0x01, 0x02, 0x04, 0x00 },	// '
	{ 0x00, 0x20, 0x54, 0x54, 0x54, 0x78 },	// a
	{ 0x00, 0x7F, 0x48, 0x44, 0x44, 0x38 },	// b
	{ 0x00, 0x38, 0x44, 0x44, 0x44, 0x20 },	// c
	{ 0x00, 0x38, 0x44, 0x44, 0x48, 0x7F },	// d
	{ 0x00, 0x38, 0x54, 0x54, 0x54, 0x18 },	// e
	{ 0x00, 0x08, 0x7E, 0x09, 0x01, 0x02 },	// f
	{ 0x00, 0x18, 0xA4, 0xA4, 0xA4, 0x7C },	// g
	{ 0x00, 0x7F, 0x08, 0x04, 0x04, 0x78 },	// h
	{ 0x00, 0x00, 0x44, 0x7D, 0x40, 0x00 },	// i
	{ 0x00, 0x40, 0x80, 0x84, 0x7D, 0x00 },	// j
	{ 0x00, 0x7F, 0x10, 0x28, 0x44, 0x00 },	// k
	{ 0x00, 0x00, 0x41, 0x7F, 0x40, 0x00 },	// l
	{ 0x00, 0x7C, 0x04, 0x18, 0x04, 0x78 },	// m
	{ 0x00, 0x7C, 0x08, 0x04, 0x04, 0x78 },	// n
	{ 0x00, 0x38, 0x44, 0x44, 0x44, 0x38 },	// o
	{ 0x00, 0xFC, 0x24, 0x24, 0x24, 0x18 },	// p
	{ 0x00, 0x18, 0x24, 0x24, 0x18, 0xFC },	// q
	{ 0x00, 0x7C, 0x08, 0x04, 0x04, 0x08 },	// r
	{ 0x00, 0x48, 0x54, 0x54, 0x54, 0x20 },	// s
	{ 0x00, 0x04, 0x3F, 0x44, 0x40, 0x20 },	// t
	{ 0x00, 0x3C, 0x40, 0x40, 0x20, 0x7C },	// u
	{ 0x00, 0x1C, 0x20, 0x40, 0x20, 0x1C },	// v
	{ 0x00, 0x3C, 0x40, 0x30, 0x40, 0x3C },	// w
	{ 0x00, 0x44, 0x28, 0x10, 0x28, 0x44 },	// x
	{ 0x00, 0x1C, 0xA0, 0xA0, 0xA0, 0x7C },	// y
	{ 0x00, 0x44, 0x64, 0x54, 0x4C, 0x44 },	// z
	{ 0x14, 0x14, 0x14, 0x14, 0x14, 0x14 },	// horiz lines
};


static u8 F8X16[] =
{
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,	// 0
	0x00,0x00,0x00,0xF8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x33,0x30,0x00,0x00,0x00,	//!1
	0x00,0x10,0x0C,0x06,0x10,0x0C,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,	//"2
	0x40,0xC0,0x78,0x40,0xC0,0x78,0x40,0x00,0x04,0x3F,0x04,0x04,0x3F,0x04,0x04,0x00,	//#3
	0x00,0x70,0x88,0xFC,0x08,0x30,0x00,0x00,0x00,0x18,0x20,0xFF,0x21,0x1E,0x00,0x00,	//$4
	0xF0,0x08,0xF0,0x00,0xE0,0x18,0x00,0x00,0x00,0x21,0x1C,0x03,0x1E,0x21,0x1E,0x00,	//%5
	0x00,0xF0,0x08,0x88,0x70,0x00,0x00,0x00,0x1E,0x21,0x23,0x24,0x19,0x27,0x21,0x10,	//&6
	0x10,0x16,0x0E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,	//'7
	0x00,0x00,0x00,0xE0,0x18,0x04,0x02,0x00,0x00,0x00,0x00,0x07,0x18,0x20,0x40,0x00,	//(8
	0x00,0x02,0x04,0x18,0xE0,0x00,0x00,0x00,0x00,0x40,0x20,0x18,0x07,0x00,0x00,0x00,	//)9
	0x40,0x40,0x80,0xF0,0x80,0x40,0x40,0x00,0x02,0x02,0x01,0x0F,0x01,0x02,0x02,0x00,	//*10
	0x00,0x00,0x00,0xF0,0x00,0x00,0x00,0x00,0x01,0x01,0x01,0x1F,0x01,0x01,0x01,0x00,	//+11
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xB0,0x70,0x00,0x00,0x00,0x00,0x00,	//,12
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x01,	//-13
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x30,0x00,0x00,0x00,0x00,0x00,	//.14
	0x00,0x00,0x00,0x00,0x80,0x60,0x18,0x04,0x00,0x60,0x18,0x06,0x01,0x00,0x00,0x00,	///15
	0x00,0xE0,0x10,0x08,0x08,0x10,0xE0,0x00,0x00,0x0F,0x10,0x20,0x20,0x10,0x0F,0x00,	//016
	0x00,0x10,0x10,0xF8,0x00,0x00,0x00,0x00,0x00,0x20,0x20,0x3F,0x20,0x20,0x00,0x00,	//117
	0x00,0x70,0x08,0x08,0x08,0x88,0x70,0x00,0x00,0x30,0x28,0x24,0x22,0x21,0x30,0x00,	//218
	0x00,0x30,0x08,0x88,0x88,0x48,0x30,0x00,0x00,0x18,0x20,0x20,0x20,0x11,0x0E,0x00,	//319
	0x00,0x00,0xC0,0x20,0x10,0xF8,0x00,0x00,0x00,0x07,0x04,0x24,0x24,0x3F,0x24,0x00,	//420
	0x00,0xF8,0x08,0x88,0x88,0x08,0x08,0x00,0x00,0x19,0x21,0x20,0x20,0x11,0x0E,0x00,	//521
	0x00,0xE0,0x10,0x88,0x88,0x18,0x00,0x00,0x00,0x0F,0x11,0x20,0x20,0x11,0x0E,0x00,	//622
	0x00,0x38,0x08,0x08,0xC8,0x38,0x08,0x00,0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00,	//723
	0x00,0x70,0x88,0x08,0x08,0x88,0x70,0x00,0x00,0x1C,0x22,0x21,0x21,0x22,0x1C,0x00,	//824
	0x00,0xE0,0x10,0x08,0x08,0x10,0xE0,0x00,0x00,0x00,0x31,0x22,0x22,0x11,0x0F,0x00,	//925
	0x00,0x00,0x00,0xC0,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x30,0x00,0x00,0x00,	//:26
	0x00,0x00,0x00,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x60,0x00,0x00,0x00,0x00,	//;27
	0x00,0x00,0x80,0x40,0x20,0x10,0x08,0x00,0x00,0x01,0x02,0x04,0x08,0x10,0x20,0x00,	//<28
	0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x00,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x00,	//=29
	0x00,0x08,0x10,0x20,0x40,0x80,0x00,0x00,0x00,0x20,0x10,0x08,0x04,0x02,0x01,0x00,	//>30
	0x00,0x70,0x48,0x08,0x08,0x08,0xF0,0x00,0x00,0x00,0x00,0x30,0x36,0x01,0x00,0x00,	//?31
	0xC0,0x30,0xC8,0x28,0xE8,0x10,0xE0,0x00,0x07,0x18,0x27,0x24,0x23,0x14,0x0B,0x00,	//@32
	0x00,0x00,0xC0,0x38,0xE0,0x00,0x00,0x00,0x20,0x3C,0x23,0x02,0x02,0x27,0x38,0x20,	//A33
	0x08,0xF8,0x88,0x88,0x88,0x70,0x00,0x00,0x20,0x3F,0x20,0x20,0x20,0x11,0x0E,0x00,	//B34
	0xC0,0x30,0x08,0x08,0x08,0x08,0x38,0x00,0x07,0x18,0x20,0x20,0x20,0x10,0x08,0x00,	//C35
	0x08,0xF8,0x08,0x08,0x08,0x10,0xE0,0x00,0x20,0x3F,0x20,0x20,0x20,0x10,0x0F,0x00,	//D36
	0x08,0xF8,0x88,0x88,0xE8,0x08,0x10,0x00,0x20,0x3F,0x20,0x20,0x23,0x20,0x18,0x00,	//E37
	0x08,0xF8,0x88,0x88,0xE8,0x08,0x10,0x00,0x20,0x3F,0x20,0x00,0x03,0x00,0x00,0x00,	//F38
	0xC0,0x30,0x08,0x08,0x08,0x38,0x00,0x00,0x07,0x18,0x20,0x20,0x22,0x1E,0x02,0x00,	//G39
	0x08,0xF8,0x08,0x00,0x00,0x08,0xF8,0x08,0x20,0x3F,0x21,0x01,0x01,0x21,0x3F,0x20,	//H40
	0x00,0x08,0x08,0xF8,0x08,0x08,0x00,0x00,0x00,0x20,0x20,0x3F,0x20,0x20,0x00,0x00,	//I41
	0x00,0x00,0x08,0x08,0xF8,0x08,0x08,0x00,0xC0,0x80,0x80,0x80,0x7F,0x00,0x00,0x00,	//J42
	0x08,0xF8,0x88,0xC0,0x28,0x18,0x08,0x00,0x20,0x3F,0x20,0x01,0x26,0x38,0x20,0x00,	//K43
	0x08,0xF8,0x08,0x00,0x00,0x00,0x00,0x00,0x20,0x3F,0x20,0x20,0x20,0x20,0x30,0x00,	//L44
	0x08,0xF8,0xF8,0x00,0xF8,0xF8,0x08,0x00,0x20,0x3F,0x00,0x3F,0x00,0x3F,0x20,0x00,	//M45
	0x08,0xF8,0x30,0xC0,0x00,0x08,0xF8,0x08,0x20,0x3F,0x20,0x00,0x07,0x18,0x3F,0x00,	//N46
	0xE0,0x10,0x08,0x08,0x08,0x10,0xE0,0x00,0x0F,0x10,0x20,0x20,0x20,0x10,0x0F,0x00,	//O47
	0x08,0xF8,0x08,0x08,0x08,0x08,0xF0,0x00,0x20,0x3F,0x21,0x01,0x01,0x01,0x00,0x00,	//P48
	0xE0,0x10,0x08,0x08,0x08,0x10,0xE0,0x00,0x0F,0x18,0x24,0x24,0x38,0x50,0x4F,0x00,	//Q49
	0x08,0xF8,0x88,0x88,0x88,0x88,0x70,0x00,0x20,0x3F,0x20,0x00,0x03,0x0C,0x30,0x20,	//R50
	0x00,0x70,0x88,0x08,0x08,0x08,0x38,0x00,0x00,0x38,0x20,0x21,0x21,0x22,0x1C,0x00,	//S51
	0x18,0x08,0x08,0xF8,0x08,0x08,0x18,0x00,0x00,0x00,0x20,0x3F,0x20,0x00,0x00,0x00,	//T52
	0x08,0xF8,0x08,0x00,0x00,0x08,0xF8,0x08,0x00,0x1F,0x20,0x20,0x20,0x20,0x1F,0x00,	//U53
	0x08,0x78,0x88,0x00,0x00,0xC8,0x38,0x08,0x00,0x00,0x07,0x38,0x0E,0x01,0x00,0x00,	//V54
	0xF8,0x08,0x00,0xF8,0x00,0x08,0xF8,0x00,0x03,0x3C,0x07,0x00,0x07,0x3C,0x03,0x00,	//W55
	0x08,0x18,0x68,0x80,0x80,0x68,0x18,0x08,0x20,0x30,0x2C,0x03,0x03,0x2C,0x30,0x20,	//X56
	0x08,0x38,0xC8,0x00,0xC8,0x38,0x08,0x00,0x00,0x00,0x20,0x3F,0x20,0x00,0x00,0x00,	//Y57
	0x10,0x08,0x08,0x08,0xC8,0x38,0x08,0x00,0x20,0x38,0x26,0x21,0x20,0x20,0x18,0x00,	//Z58
	0x00,0x00,0x00,0xFE,0x02,0x02,0x02,0x00,0x00,0x00,0x00,0x7F,0x40,0x40,0x40,0x00,	//[59
	0x00,0x0C,0x30,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x06,0x38,0xC0,0x00,	//\60
	0x00,0x02,0x02,0x02,0xFE,0x00,0x00,0x00,0x00,0x40,0x40,0x40,0x7F,0x00,0x00,0x00,	//]61
	0x00,0x00,0x04,0x02,0x02,0x02,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,	//^62
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,	//_63
	0x00,0x02,0x02,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,	//`64
	0x00,0x00,0x80,0x80,0x80,0x80,0x00,0x00,0x00,0x19,0x24,0x22,0x22,0x22,0x3F,0x20,	//a65
	0x08,0xF8,0x00,0x80,0x80,0x00,0x00,0x00,0x00,0x3F,0x11,0x20,0x20,0x11,0x0E,0x00,	//b66
	0x00,0x00,0x00,0x80,0x80,0x80,0x00,0x00,0x00,0x0E,0x11,0x20,0x20,0x20,0x11,0x00,	//c67
	0x00,0x00,0x00,0x80,0x80,0x88,0xF8,0x00,0x00,0x0E,0x11,0x20,0x20,0x10,0x3F,0x20,	//d68
	0x00,0x00,0x80,0x80,0x80,0x80,0x00,0x00,0x00,0x1F,0x22,0x22,0x22,0x22,0x13,0x00,	//e69
	0x00,0x80,0x80,0xF0,0x88,0x88,0x88,0x18,0x00,0x20,0x20,0x3F,0x20,0x20,0x00,0x00,	//f70
	0x00,0x00,0x80,0x80,0x80,0x80,0x80,0x00,0x00,0x6B,0x94,0x94,0x94,0x93,0x60,0x00,	//g71
	0x08,0xF8,0x00,0x80,0x80,0x80,0x00,0x00,0x20,0x3F,0x21,0x00,0x00,0x20,0x3F,0x20,	//h72
	0x00,0x80,0x98,0x98,0x00,0x00,0x00,0x00,0x00,0x20,0x20,0x3F,0x20,0x20,0x00,0x00,	//i73
	0x00,0x00,0x00,0x80,0x98,0x98,0x00,0x00,0x00,0xC0,0x80,0x80,0x80,0x7F,0x00,0x00,	//j74
	0x08,0xF8,0x00,0x00,0x80,0x80,0x80,0x00,0x20,0x3F,0x24,0x02,0x2D,0x30,0x20,0x00,	//k75
	0x00,0x08,0x08,0xF8,0x00,0x00,0x00,0x00,0x00,0x20,0x20,0x3F,0x20,0x20,0x00,0x00,	//l76
	0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x00,0x20,0x3F,0x20,0x00,0x3F,0x20,0x00,0x3F,	//m77
	0x80,0x80,0x00,0x80,0x80,0x80,0x00,0x00,0x20,0x3F,0x21,0x00,0x00,0x20,0x3F,0x20,	//n78
	0x00,0x00,0x80,0x80,0x80,0x80,0x00,0x00,0x00,0x1F,0x20,0x20,0x20,0x20,0x1F,0x00,	//o79
	0x80,0x80,0x00,0x80,0x80,0x00,0x00,0x00,0x80,0xFF,0xA1,0x20,0x20,0x11,0x0E,0x00,	//p80
	0x00,0x00,0x00,0x80,0x80,0x80,0x80,0x00,0x00,0x0E,0x11,0x20,0x20,0xA0,0xFF,0x80,	//q81
	0x80,0x80,0x80,0x00,0x80,0x80,0x80,0x00,0x20,0x20,0x3F,0x21,0x20,0x00,0x01,0x00,	//r82
	0x00,0x00,0x80,0x80,0x80,0x80,0x80,0x00,0x00,0x33,0x24,0x24,0x24,0x24,0x19,0x00,	//s83
	0x00,0x80,0x80,0xE0,0x80,0x80,0x00,0x00,0x00,0x00,0x00,0x1F,0x20,0x20,0x00,0x00,	//t84
	0x80,0x80,0x00,0x00,0x00,0x80,0x80,0x00,0x00,0x1F,0x20,0x20,0x20,0x10,0x3F,0x20,	//u85
	0x80,0x80,0x80,0x00,0x00,0x80,0x80,0x80,0x00,0x01,0x0E,0x30,0x08,0x06,0x01,0x00,	//v86
	0x80,0x80,0x00,0x80,0x00,0x80,0x80,0x80,0x0F,0x30,0x0C,0x03,0x0C,0x30,0x0F,0x00,	//w87
	0x00,0x80,0x80,0x00,0x80,0x80,0x80,0x00,0x00,0x20,0x31,0x2E,0x0E,0x31,0x20,0x00,	//x88
	0x80,0x80,0x80,0x00,0x00,0x80,0x80,0x80,0x80,0x81,0x8E,0x70,0x18,0x06,0x01,0x00,	//y89
	0x00,0x80,0x80,0x80,0x80,0x80,0x80,0x00,0x00,0x21,0x30,0x2C,0x22,0x21,0x30,0x00,	//z90
	0x00,0x00,0x00,0x00,0x80,0x7C,0x02,0x02,0x00,0x00,0x00,0x00,0x00,0x3F,0x40,0x40,	//{91
	0x00,0x00,0x00,0x00,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0x00,0x00,	//|92
	0x00,0x02,0x02,0x7C,0x80,0x00,0x00,0x00,0x00,0x40,0x40,0x3F,0x00,0x00,0x00,0x00,	//}93
	0x00,0x06,0x01,0x01,0x02,0x02,0x04,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,	//~94
};

struct lq12864_data {
	struct mcu_device *device;
	struct mutex lock;
} *lq12864 = NULL;


static s32 lq12864_ioctl_fill(struct mcu_device *device, u8 what)
{
	unsigned char buffer[] = {what};
	return mcu_device_command(device, 'F', buffer, sizeof(buffer)) < 0;
}

static s32 lq12864_ioctl_clear(struct mcu_device *device)
{
	return lq12864_ioctl_fill(device, 0);
}

struct mcu_protocol_draw {
	u8 x;
	u8 width;
	u8 width2;
	u8 y:3;
	u8 inverse:1;
	u8 height:4;
	u8 data[0];
};

static s32 lq12864_ioctl_draw(struct mcu_device *device, struct lq12864_ioctl_data *data)
{
	u32 ret = 0;
	struct mcu_protocol_draw *draw;

	/* check param */
	if (data->x >= LQ12864_WIDTH
		|| data->y >= LQ12864_HEIGHT) {
		pr_err("%s: x y check failed\n", __func__);
		return -EINVAL;
	}
	if (data->x + data->width2 > LQ12864_WIDTH
		|| data->y + data->height > LQ12864_HEIGHT
		|| data->width2 > data->width) {
		pr_err("%s: width height check failed\n", __func__);
		return -EINVAL;
	}
	if (data->size != data->width * data->height) {
		pr_err("%s: size check failed\n", __func__);
		return -EINVAL;
	}

	draw = kzalloc(sizeof(struct mcu_protocol_draw) + data->size, GFP_KERNEL);
	if (NULL == draw) {
		return -ENOMEM;
	}

	draw->x = data->x;
	draw->width = data->width;
	draw->width2 = data->width2;
	draw->y = data->y;
	draw->inverse = data->inverse;
	draw->height = data->height;
	memcpy(&draw->data, data->data, data->size);

	ret = mcu_device_command(device, 'D', (unsigned char *)draw, sizeof(struct mcu_protocol_draw) + data->size);

	kfree(draw);

	return ret < 0;
}

static s32 lq12864_ioctl_en(struct mcu_device *device, struct lq12864_ioctl_data *data)
{
	u32 ret = 0;
	u8 i, ch;
	struct lq12864_ioctl_data param = {
		.width = 6,
		.height = 1,
		.size = 6,
		.width2 = 6,
	};

	/* check param */
	if (data->x >= LQ12864_WIDTH
		|| data->y >= LQ12864_HEIGHT) {
		return -EINVAL;
	}

	param.x = data->x;
	param.y = data->y;
	param.inverse = data->inverse;

	for (i = 0; i < data->size; i++) {
		ch = data->data[i];
		if (ch < 32) continue;
		ch -= 32;

		param.data = F6x8[ch];
		if (param.x > LQ12864_WIDTH - param.width2) {
			param.x = 0;
			param.y += param.height;
			if (param.y > LQ12864_HEIGHT - param.height)
				break;
		}

		ret = lq12864_ioctl_draw(device, &param);
		if (0 != ret) return ret;

		param.x += param.width2;
	}

	return 0;
}

static s32 lq12864_ioctl_en2(struct mcu_device *device, struct lq12864_ioctl_data *data)
{
	u32 ret = 0;
	u8 i, ch;
	struct lq12864_ioctl_data param = {
		.width = 8,
		.height = 2,
		.size = 16,
		.width2 = 8,
	};

	/* check param */
	if (data->x >= LQ12864_WIDTH
		|| data->y >= LQ12864_HEIGHT) {
		return -EINVAL;
	}

	param.x = data->x;
	param.y = data->y;
	param.inverse = data->inverse;

	for (i = 0; i < data->size; i++) {
		ch = data->data[i];
		if (ch < 32) continue;
		ch -= 32;

		param.data = &F8X16[ch * param.size];
		if (param.x > LQ12864_WIDTH - param.width2) {
			param.x = 0;
			param.y += param.height;
			if (param.y > LQ12864_HEIGHT - param.height)
				break;
		}

		ret = lq12864_ioctl_draw(device, &param);
		if (0 != ret) return ret;

		param.x += param.width2;
	}

	return 0;
}

static long lq12864_ioctl(struct mcu_device *device, unsigned int cmd, struct lq12864_ioctl_data __user *arg)
{
	struct lq12864_ioctl_data param;
	u8 *buffer = NULL;
	s32 ret = 0;
	struct lq12864_data *data = mcu_get_drvdata(device);

	if (_IOC_SIZE(cmd) != sizeof(arg)) {
		pr_err("%s: ioctl param size mismatch: %d != %d\n", __func__, _IOC_SIZE(cmd), sizeof(param));
		return -EINVAL;
	}

	if (copy_from_user(&param, arg, sizeof(param))) {
		pr_err("%s: copy param from user failed\n", __func__);
		return -EFAULT;
	}

	if (param.size) {
		buffer = kzalloc(param.size, GFP_KERNEL);
		if (NULL == buffer) {
			pr_err("%s: kzalloc failed\n", __func__);
			return -ENOMEM;
		}

		if (copy_from_user(buffer, param.data, param.size)) {
			pr_err("%s: copy param data from user failed\n", __func__);
			return -EFAULT;
		}

		param.data = buffer;
	}

	mutex_lock(&data->lock);

	switch (cmd) {
	case LQ12864_IOCTL_INIT:
		ret = 0;
		break;
	case LQ12864_IOCTL_CLEAR:
		ret = lq12864_ioctl_clear(device);
		break;
	case LQ12864_IOCTL_FILL:
		if (param.size)
			ret = lq12864_ioctl_fill(device, buffer[0]);
		break;
	case LQ12864_IOCTL_DRAW:
		ret = lq12864_ioctl_draw(device, &param);
		break;
	case LQ12864_IOCTL_EN:
		ret = lq12864_ioctl_en(device, &param);
		break;
	case LQ12864_IOCTL_EN2:
		ret = lq12864_ioctl_en2(device, &param);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&data->lock);

	if (NULL != buffer)
		kfree(buffer);

	return ret;
}

static long lq12864_fs_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct lq12864_data *data = (struct lq12864_data *)file->private_data;
	return lq12864_ioctl(data->device, cmd, (struct lq12864_ioctl_data __user *)arg);
}

static int lq12864_open(struct inode *inode, struct file *file)
{
	int ret = nonseekable_open(inode, file);
	if (ret)
		return ret;

	file->private_data = lq12864;
	return 0;
}

static int lq12864_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations lq12864_fops = {
	.owner = THIS_MODULE,
	.open = lq12864_open,
	.release = lq12864_release,
	.unlocked_ioctl = lq12864_fs_ioctl,
};

static struct miscdevice lq12864_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "oled",
	.fops = &lq12864_fops,
};

static int mcu_oled_probe(struct mcu_device *device, const struct mcu_device_id *id)
{
	struct device *dev = &device->dev;
	struct lq12864_data *state;
	int ret = -ENODEV;

	switch (id->driver_data) {
	case 0:
		state = kzalloc(sizeof(struct lq12864_data), GFP_KERNEL);
		if (NULL == state) {
			dev_err(dev, "failed to create state\n");
			ret = -ENOMEM;
			goto exit_alloc_data_failed;
		}

		{
			mutex_init(&state->lock);
			mcu_set_drvdata(device, state);
			state->device = device;

			ret = misc_register(&lq12864_device);
			if (ret) {
				pr_err("mcu-oled: lq12864_device register failed\n");
				goto exit_misc_device_reg_failed;
			}

			dev_info(dev, "lq12864 device created\n");
			lq12864 = state;
			ret = 0;
		}
		break;
	default:
		break;
	}

	return ret;

exit_misc_device_reg_failed:
	mutex_destroy(&state->lock);
	kfree(state);
exit_alloc_data_failed:
	return ret;
}

static int mcu_oled_remove(struct mcu_device *device)
{
	struct lq12864_data *state = mcu_get_drvdata(device);

	misc_deregister(&lq12864_device);
	mutex_destroy(&state->lock);
	kfree(state);
	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id mcu_oled_dt_match[] = {
	{ .compatible = "lbs,mcu-oled" },
	{ },
};
MODULE_DEVICE_TABLE(of, mcu_oled_dt_match);
#endif

static struct mcu_device_id mcu_oled_id[] = {
	{ "mcu-oled", 0 },
	{ }
};

struct mcu_driver __mcu_oled = {
	.driver	= {
		.name	= "mcu-oled",
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = of_match_ptr(mcu_oled_dt_match),
#endif
	},
	.probe	= mcu_oled_probe,
	.remove	= mcu_oled_remove,
	.id_table	= mcu_oled_id,
};

