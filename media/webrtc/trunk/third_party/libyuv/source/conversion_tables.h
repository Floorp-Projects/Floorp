/*
 *  Copyright (c) 2011 The LibYuv project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/**************************************************************
*  conversion_tables.h
*
*    Pre-compiled definitions of the conversion equations: YUV -> RGB.
*
***************************************************************/

#ifndef LIBYUV_SOURCE_CONVERSION_TABLES_H_
#define LIBYUV_SOURCE_CONVERSION_TABLES_H_

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

/******************************************************************************
* YUV TO RGB approximation
*
*  R = clip( (298 * (Y - 16)                   + 409 * (V - 128) + 128 ) >> 8 )
*  G = clip( (298 * (Y - 16) - 100 * (U - 128) - 208 * (V - 128) + 128 ) >> 8 )
*  B = clip( (298 * (Y - 16) + 516 * (U - 128)                   + 128 ) >> 8 )
*******************************************************************************/

    #define Yc(i)  static_cast<int> ( 298  * ( i - 16 )) // Y contribution
    #define Ucg(i) static_cast<int> ( -100 * ( i - 128 ))// U contribution to G
    #define Ucb(i) static_cast<int> ( 516  * ( i - 128 ))// U contribution to B
    #define Vcr(i) static_cast<int> ( 409  * ( i - 128 ))// V contribution to R
    #define Vcg(i) static_cast<int> ( -208 * ( i - 128 ))// V contribution to G

    static const int mapYc[256] = {
        Yc(0),Yc(1),Yc(2),Yc(3),Yc(4),Yc(5),Yc(6),Yc(7),Yc(8),Yc(9),
        Yc(10),Yc(11),Yc(12),Yc(13),Yc(14),Yc(15),Yc(16),Yc(17),Yc(18),Yc(19),
        Yc(20),Yc(21),Yc(22),Yc(23),Yc(24),Yc(25),Yc(26),Yc(27),Yc(28),Yc(29),
        Yc(30),Yc(31),Yc(32),Yc(33),Yc(34),Yc(35),Yc(36),Yc(37),Yc(38),Yc(39),
        Yc(40),Yc(41),Yc(42),Yc(43),Yc(44),Yc(45),Yc(46),Yc(47),Yc(48),Yc(49),
        Yc(50),Yc(51),Yc(52),Yc(53),Yc(54),Yc(55),Yc(56),Yc(57),Yc(58),Yc(59),
        Yc(60),Yc(61),Yc(62),Yc(63),Yc(64),Yc(65),Yc(66),Yc(67),Yc(68),Yc(69),
        Yc(70),Yc(71),Yc(72),Yc(73),Yc(74),Yc(75),Yc(76),Yc(77),Yc(78),Yc(79),
        Yc(80),Yc(81),Yc(82),Yc(83),Yc(84),Yc(85),Yc(86),Yc(87),Yc(88),Yc(89),
        Yc(90),Yc(91),Yc(92),Yc(93),Yc(94),Yc(95),Yc(96),Yc(97),Yc(98),Yc(99),
        Yc(100),Yc(101),Yc(102),Yc(103),Yc(104),Yc(105),Yc(106),Yc(107),Yc(108),
        Yc(109),Yc(110),Yc(111),Yc(112),Yc(113),Yc(114),Yc(115),Yc(116),Yc(117),
        Yc(118),Yc(119),Yc(120),Yc(121),Yc(122),Yc(123),Yc(124),Yc(125),Yc(126),
        Yc(127),Yc(128),Yc(129),Yc(130),Yc(131),Yc(132),Yc(133),Yc(134),Yc(135),
        Yc(136),Yc(137),Yc(138),Yc(139),Yc(140),Yc(141),Yc(142),Yc(143),Yc(144),
        Yc(145),Yc(146),Yc(147),Yc(148),Yc(149),Yc(150),Yc(151),Yc(152),Yc(153),
        Yc(154),Yc(155),Yc(156),Yc(157),Yc(158),Yc(159),Yc(160),Yc(161),Yc(162),
        Yc(163),Yc(164),Yc(165),Yc(166),Yc(167),Yc(168),Yc(169),Yc(170),Yc(171),
        Yc(172),Yc(173),Yc(174),Yc(175),Yc(176),Yc(177),Yc(178),Yc(179),Yc(180),
        Yc(181),Yc(182),Yc(183),Yc(184),Yc(185),Yc(186),Yc(187),Yc(188),Yc(189),
        Yc(190),Yc(191),Yc(192),Yc(193),Yc(194),Yc(195),Yc(196),Yc(197),Yc(198),
        Yc(199),Yc(200),Yc(201),Yc(202),Yc(203),Yc(204),Yc(205),Yc(206),Yc(207),
        Yc(208),Yc(209),Yc(210),Yc(211),Yc(212),Yc(213),Yc(214),Yc(215),Yc(216),
        Yc(217),Yc(218),Yc(219),Yc(220),Yc(221),Yc(222),Yc(223),Yc(224),Yc(225),
        Yc(226),Yc(227),Yc(228),Yc(229),Yc(230),Yc(231),Yc(232),Yc(233),Yc(234),
        Yc(235),Yc(236),Yc(237),Yc(238),Yc(239),Yc(240),Yc(241),Yc(242),Yc(243),
        Yc(244),Yc(245),Yc(246),Yc(247),Yc(248),Yc(249),Yc(250),Yc(251),Yc(252),
        Yc(253),Yc(254),Yc(255)};

   static const int mapUcg[256] = {
        Ucg(0),Ucg(1),Ucg(2),Ucg(3),Ucg(4),Ucg(5),Ucg(6),Ucg(7),Ucg(8),Ucg(9),
        Ucg(10),Ucg(11),Ucg(12),Ucg(13),Ucg(14),Ucg(15),Ucg(16),Ucg(17),Ucg(18),
        Ucg(19),Ucg(20),Ucg(21),Ucg(22),Ucg(23),Ucg(24),Ucg(25),Ucg(26),Ucg(27),
        Ucg(28),Ucg(29),Ucg(30),Ucg(31),Ucg(32),Ucg(33),Ucg(34),Ucg(35),Ucg(36),
        Ucg(37),Ucg(38),Ucg(39),Ucg(40),Ucg(41),Ucg(42),Ucg(43),Ucg(44),Ucg(45),
        Ucg(46),Ucg(47),Ucg(48),Ucg(49),Ucg(50),Ucg(51),Ucg(52),Ucg(53),Ucg(54),
        Ucg(55),Ucg(56),Ucg(57),Ucg(58),Ucg(59),Ucg(60),Ucg(61),Ucg(62),Ucg(63),
        Ucg(64),Ucg(65),Ucg(66),Ucg(67),Ucg(68),Ucg(69),Ucg(70),Ucg(71),Ucg(72),
        Ucg(73),Ucg(74),Ucg(75),Ucg(76),Ucg(77),Ucg(78),Ucg(79),Ucg(80),Ucg(81),
        Ucg(82),Ucg(83),Ucg(84),Ucg(85),Ucg(86),Ucg(87),Ucg(88),Ucg(89),Ucg(90),
        Ucg(91),Ucg(92),Ucg(93),Ucg(94),Ucg(95),Ucg(96),Ucg(97),Ucg(98),Ucg(99),
        Ucg(100),Ucg(101),Ucg(102),Ucg(103),Ucg(104),Ucg(105),Ucg(106),Ucg(107),
        Ucg(108),Ucg(109),Ucg(110),Ucg(111),Ucg(112),Ucg(113),Ucg(114),Ucg(115),
        Ucg(116),Ucg(117),Ucg(118),Ucg(119),Ucg(120),Ucg(121),Ucg(122),Ucg(123),
        Ucg(124),Ucg(125),Ucg(126),Ucg(127),Ucg(128),Ucg(129),Ucg(130),Ucg(131),
        Ucg(132),Ucg(133),Ucg(134),Ucg(135),Ucg(136),Ucg(137),Ucg(138),Ucg(139),
        Ucg(140),Ucg(141),Ucg(142),Ucg(143),Ucg(144),Ucg(145),Ucg(146),Ucg(147),
        Ucg(148),Ucg(149),Ucg(150),Ucg(151),Ucg(152),Ucg(153),Ucg(154),Ucg(155),
        Ucg(156),Ucg(157),Ucg(158),Ucg(159),Ucg(160),Ucg(161),Ucg(162),Ucg(163),
        Ucg(164),Ucg(165),Ucg(166),Ucg(167),Ucg(168),Ucg(169),Ucg(170),Ucg(171),
        Ucg(172),Ucg(173),Ucg(174),Ucg(175),Ucg(176),Ucg(177),Ucg(178),Ucg(179),
        Ucg(180),Ucg(181),Ucg(182),Ucg(183),Ucg(184),Ucg(185),Ucg(186),Ucg(187),
        Ucg(188),Ucg(189),Ucg(190),Ucg(191),Ucg(192),Ucg(193),Ucg(194),Ucg(195),
        Ucg(196),Ucg(197),Ucg(198),Ucg(199),Ucg(200),Ucg(201),Ucg(202),Ucg(203),
        Ucg(204),Ucg(205),Ucg(206),Ucg(207),Ucg(208),Ucg(209),Ucg(210),Ucg(211),
        Ucg(212),Ucg(213),Ucg(214),Ucg(215),Ucg(216),Ucg(217),Ucg(218),Ucg(219),
        Ucg(220),Ucg(221),Ucg(222),Ucg(223),Ucg(224),Ucg(225),Ucg(226),Ucg(227),
        Ucg(228),Ucg(229),Ucg(230),Ucg(231),Ucg(232),Ucg(233),Ucg(234),Ucg(235),
        Ucg(236),Ucg(237),Ucg(238),Ucg(239),Ucg(240),Ucg(241),Ucg(242),Ucg(243),
        Ucg(244),Ucg(245),Ucg(246),Ucg(247),Ucg(248),Ucg(249),Ucg(250),Ucg(251),
        Ucg(252),Ucg(253),Ucg(254),Ucg(255)};

   static const int mapUcb[256] = {
        Ucb(0),Ucb(1),Ucb(2),Ucb(3),Ucb(4),Ucb(5),Ucb(6),Ucb(7),Ucb(8),Ucb(9),
        Ucb(10),Ucb(11),Ucb(12),Ucb(13),Ucb(14),Ucb(15),Ucb(16),Ucb(17),Ucb(18),
        Ucb(19),Ucb(20),Ucb(21),Ucb(22),Ucb(23),Ucb(24),Ucb(25),Ucb(26),Ucb(27),
        Ucb(28),Ucb(29),Ucb(30),Ucb(31),Ucb(32),Ucb(33),Ucb(34),Ucb(35),Ucb(36),
        Ucb(37),Ucb(38),Ucb(39),Ucb(40),Ucb(41),Ucb(42),Ucb(43),Ucb(44),Ucb(45),
        Ucb(46),Ucb(47),Ucb(48),Ucb(49),Ucb(50),Ucb(51),Ucb(52),Ucb(53),Ucb(54),
        Ucb(55),Ucb(56),Ucb(57),Ucb(58),Ucb(59),Ucb(60),Ucb(61),Ucb(62),Ucb(63),
        Ucb(64),Ucb(65),Ucb(66),Ucb(67),Ucb(68),Ucb(69),Ucb(70),Ucb(71),Ucb(72),
        Ucb(73),Ucb(74),Ucb(75),Ucb(76),Ucb(77),Ucb(78),Ucb(79),Ucb(80),Ucb(81),
        Ucb(82),Ucb(83),Ucb(84),Ucb(85),Ucb(86),Ucb(87),Ucb(88),Ucb(89),Ucb(90),
        Ucb(91),Ucb(92),Ucb(93),Ucb(94),Ucb(95),Ucb(96),Ucb(97),Ucb(98),Ucb(99),
        Ucb(100),Ucb(101),Ucb(102),Ucb(103),Ucb(104),Ucb(105),Ucb(106),Ucb(107),
        Ucb(108),Ucb(109),Ucb(110),Ucb(111),Ucb(112),Ucb(113),Ucb(114),Ucb(115),
        Ucb(116),Ucb(117),Ucb(118),Ucb(119),Ucb(120),Ucb(121),Ucb(122),Ucb(123),
        Ucb(124),Ucb(125),Ucb(126),Ucb(127),Ucb(128),Ucb(129),Ucb(130),Ucb(131),
        Ucb(132),Ucb(133),Ucb(134),Ucb(135),Ucb(136),Ucb(137),Ucb(138),Ucb(139),
        Ucb(140),Ucb(141),Ucb(142),Ucb(143),Ucb(144),Ucb(145),Ucb(146),Ucb(147),
        Ucb(148),Ucb(149),Ucb(150),Ucb(151),Ucb(152),Ucb(153),Ucb(154),Ucb(155),
        Ucb(156),Ucb(157),Ucb(158),Ucb(159),Ucb(160),Ucb(161),Ucb(162),Ucb(163),
        Ucb(164),Ucb(165),Ucb(166),Ucb(167),Ucb(168),Ucb(169),Ucb(170),Ucb(171),
        Ucb(172),Ucb(173),Ucb(174),Ucb(175),Ucb(176),Ucb(177),Ucb(178),Ucb(179),
        Ucb(180),Ucb(181),Ucb(182),Ucb(183),Ucb(184),Ucb(185),Ucb(186),Ucb(187),
        Ucb(188),Ucb(189),Ucb(190),Ucb(191),Ucb(192),Ucb(193),Ucb(194),Ucb(195),
        Ucb(196),Ucb(197),Ucb(198),Ucb(199),Ucb(200),Ucb(201),Ucb(202),Ucb(203),
        Ucb(204),Ucb(205),Ucb(206),Ucb(207),Ucb(208),Ucb(209),Ucb(210),Ucb(211),
        Ucb(212),Ucb(213),Ucb(214),Ucb(215),Ucb(216),Ucb(217),Ucb(218),Ucb(219),
        Ucb(220),Ucb(221),Ucb(222),Ucb(223),Ucb(224),Ucb(225),Ucb(226),Ucb(227),
        Ucb(228),Ucb(229),Ucb(230),Ucb(231),Ucb(232),Ucb(233),Ucb(234),Ucb(235),
        Ucb(236),Ucb(237),Ucb(238),Ucb(239),Ucb(240),Ucb(241),Ucb(242),Ucb(243),
        Ucb(244),Ucb(245),Ucb(246),Ucb(247),Ucb(248),Ucb(249),Ucb(250),Ucb(251),
        Ucb(252),Ucb(253),Ucb(254),Ucb(255)};

    static const int mapVcr[256] = {
        Vcr(0),Vcr(1),Vcr(2),Vcr(3),Vcr(4),Vcr(5),Vcr(6),Vcr(7),Vcr(8),Vcr(9),
        Vcr(10),Vcr(11),Vcr(12),Vcr(13),Vcr(14),Vcr(15),Vcr(16),Vcr(17),Vcr(18),
        Vcr(19),Vcr(20),Vcr(21),Vcr(22),Vcr(23),Vcr(24),Vcr(25),Vcr(26),Vcr(27),
        Vcr(28),Vcr(29),Vcr(30),Vcr(31),Vcr(32),Vcr(33),Vcr(34),Vcr(35),Vcr(36),
        Vcr(37),Vcr(38),Vcr(39),Vcr(40),Vcr(41),Vcr(42),Vcr(43),Vcr(44),Vcr(45),
        Vcr(46),Vcr(47),Vcr(48),Vcr(49),Vcr(50),Vcr(51),Vcr(52),Vcr(53),Vcr(54),
        Vcr(55),Vcr(56),Vcr(57),Vcr(58),Vcr(59),Vcr(60),Vcr(61),Vcr(62),Vcr(63),
        Vcr(64),Vcr(65),Vcr(66),Vcr(67),Vcr(68),Vcr(69),Vcr(70),Vcr(71),Vcr(72),
        Vcr(73),Vcr(74),Vcr(75),Vcr(76),Vcr(77),Vcr(78),Vcr(79),Vcr(80),Vcr(81),
        Vcr(82),Vcr(83),Vcr(84),Vcr(85),Vcr(86),Vcr(87),Vcr(88),Vcr(89),Vcr(90),
        Vcr(91),Vcr(92),Vcr(93),Vcr(94),Vcr(95),Vcr(96),Vcr(97),Vcr(98),Vcr(99),
        Vcr(100),Vcr(101),Vcr(102),Vcr(103),Vcr(104),Vcr(105),Vcr(106),Vcr(107),
        Vcr(108),Vcr(109),Vcr(110),Vcr(111),Vcr(112),Vcr(113),Vcr(114),Vcr(115),
        Vcr(116),Vcr(117),Vcr(118),Vcr(119),Vcr(120),Vcr(121),Vcr(122),Vcr(123),
        Vcr(124),Vcr(125),Vcr(126),Vcr(127),Vcr(128),Vcr(129),Vcr(130),Vcr(131),
        Vcr(132),Vcr(133),Vcr(134),Vcr(135),Vcr(136),Vcr(137),Vcr(138),Vcr(139),
        Vcr(140),Vcr(141),Vcr(142),Vcr(143),Vcr(144),Vcr(145),Vcr(146),Vcr(147),
        Vcr(148),Vcr(149),Vcr(150),Vcr(151),Vcr(152),Vcr(153),Vcr(154),Vcr(155),
        Vcr(156),Vcr(157),Vcr(158),Vcr(159),Vcr(160),Vcr(161),Vcr(162),Vcr(163),
        Vcr(164),Vcr(165),Vcr(166),Vcr(167),Vcr(168),Vcr(169),Vcr(170),Vcr(171),
        Vcr(172),Vcr(173),Vcr(174),Vcr(175),Vcr(176),Vcr(177),Vcr(178),Vcr(179),
        Vcr(180),Vcr(181),Vcr(182),Vcr(183),Vcr(184),Vcr(185),Vcr(186),Vcr(187),
        Vcr(188),Vcr(189),Vcr(190),Vcr(191),Vcr(192),Vcr(193),Vcr(194),Vcr(195),
        Vcr(196),Vcr(197),Vcr(198),Vcr(199),Vcr(200),Vcr(201),Vcr(202),Vcr(203),
        Vcr(204),Vcr(205),Vcr(206),Vcr(207),Vcr(208),Vcr(209),Vcr(210),Vcr(211),
        Vcr(212),Vcr(213),Vcr(214),Vcr(215),Vcr(216),Vcr(217),Vcr(218),Vcr(219),
        Vcr(220),Vcr(221),Vcr(222),Vcr(223),Vcr(224),Vcr(225),Vcr(226),Vcr(227),
        Vcr(228),Vcr(229),Vcr(230),Vcr(231),Vcr(232),Vcr(233),Vcr(234),Vcr(235),
        Vcr(236),Vcr(237),Vcr(238),Vcr(239),Vcr(240),Vcr(241),Vcr(242),Vcr(243),
        Vcr(244),Vcr(245),Vcr(246),Vcr(247),Vcr(248),Vcr(249),Vcr(250),Vcr(251),
        Vcr(252),Vcr(253),Vcr(254),Vcr(255)};


         static const int mapVcg[256] = {
        Vcg(0),Vcg(1),Vcg(2),Vcg(3),Vcg(4),Vcg(5),Vcg(6),Vcg(7),Vcg(8),Vcg(9),
        Vcg(10),Vcg(11),Vcg(12),Vcg(13),Vcg(14),Vcg(15),Vcg(16),Vcg(17),Vcg(18),
        Vcg(19),Vcg(20),Vcg(21),Vcg(22),Vcg(23),Vcg(24),Vcg(25),Vcg(26),Vcg(27),
        Vcg(28),Vcg(29),Vcg(30),Vcg(31),Vcg(32),Vcg(33),Vcg(34),Vcg(35),Vcg(36),
        Vcg(37),Vcg(38),Vcg(39),Vcg(40),Vcg(41),Vcg(42),Vcg(43),Vcg(44),Vcg(45),
        Vcg(46),Vcg(47),Vcg(48),Vcg(49),Vcg(50),Vcg(51),Vcg(52),Vcg(53),Vcg(54),
        Vcg(55),Vcg(56),Vcg(57),Vcg(58),Vcg(59),Vcg(60),Vcg(61),Vcg(62),Vcg(63),
        Vcg(64),Vcg(65),Vcg(66),Vcg(67),Vcg(68),Vcg(69),Vcg(70),Vcg(71),Vcg(72),
        Vcg(73),Vcg(74),Vcg(75),Vcg(76),Vcg(77),Vcg(78),Vcg(79),Vcg(80),Vcg(81),
        Vcg(82),Vcg(83),Vcg(84),Vcg(85),Vcg(86),Vcg(87),Vcg(88),Vcg(89),Vcg(90),
        Vcg(91),Vcg(92),Vcg(93),Vcg(94),Vcg(95),Vcg(96),Vcg(97),Vcg(98),Vcg(99),
        Vcg(100),Vcg(101),Vcg(102),Vcg(103),Vcg(104),Vcg(105),Vcg(106),Vcg(107),
        Vcg(108),Vcg(109),Vcg(110),Vcg(111),Vcg(112),Vcg(113),Vcg(114),Vcg(115),
        Vcg(116),Vcg(117),Vcg(118),Vcg(119),Vcg(120),Vcg(121),Vcg(122),Vcg(123),
        Vcg(124),Vcg(125),Vcg(126),Vcg(127),Vcg(128),Vcg(129),Vcg(130),Vcg(131),
        Vcg(132),Vcg(133),Vcg(134),Vcg(135),Vcg(136),Vcg(137),Vcg(138),Vcg(139),
        Vcg(140),Vcg(141),Vcg(142),Vcg(143),Vcg(144),Vcg(145),Vcg(146),Vcg(147),
        Vcg(148),Vcg(149),Vcg(150),Vcg(151),Vcg(152),Vcg(153),Vcg(154),Vcg(155),
        Vcg(156),Vcg(157),Vcg(158),Vcg(159),Vcg(160),Vcg(161),Vcg(162),Vcg(163),
        Vcg(164),Vcg(165),Vcg(166),Vcg(167),Vcg(168),Vcg(169),Vcg(170),Vcg(171),
        Vcg(172),Vcg(173),Vcg(174),Vcg(175),Vcg(176),Vcg(177),Vcg(178),Vcg(179),
        Vcg(180),Vcg(181),Vcg(182),Vcg(183),Vcg(184),Vcg(185),Vcg(186),Vcg(187),
        Vcg(188),Vcg(189),Vcg(190),Vcg(191),Vcg(192),Vcg(193),Vcg(194),Vcg(195),
        Vcg(196),Vcg(197),Vcg(198),Vcg(199),Vcg(200),Vcg(201),Vcg(202),Vcg(203),
        Vcg(204),Vcg(205),Vcg(206),Vcg(207),Vcg(208),Vcg(209),Vcg(210),Vcg(211),
        Vcg(212),Vcg(213),Vcg(214),Vcg(215),Vcg(216),Vcg(217),Vcg(218),Vcg(219),
        Vcg(220),Vcg(221),Vcg(222),Vcg(223),Vcg(224),Vcg(225),Vcg(226),Vcg(227),
        Vcg(228),Vcg(229),Vcg(230),Vcg(231),Vcg(232),Vcg(233),Vcg(234),Vcg(235),
        Vcg(236),Vcg(237),Vcg(238),Vcg(239),Vcg(240),Vcg(241),Vcg(242),Vcg(243),
        Vcg(244),Vcg(245),Vcg(246),Vcg(247),Vcg(248),Vcg(249),Vcg(250),Vcg(251),
        Vcg(252),Vcg(253),Vcg(254),Vcg(255)};

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif

#endif

