#############################################################################
# This file is part of pysgpp, a program package making use of spatially    #
# adaptive sparse grids to solve numerical problems                         #
#                                                                           #
# Copyright (C) 2007 Joerg Blank (blankj@in.tum.de)                         #
# Copyright (C) 2009 Alexander Heinecke (Alexander.Heinecke@mytum.de)       #
#                                                                           #
# pysgpp is free software; you can redistribute it and/or modify            #
# it under the terms of the GNU Lesser General Public License as published  #
# by the Free Software Foundation; either version 3 of the License, or      #
# (at your option) any later version.                                       #
#                                                                           #
# pysgpp is distributed in the hope that it will be useful,                 #
# but WITHOUT ANY WARRANTY; without even the implied warranty of            #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             #
# GNU Lesser General Public License for more details.                       #
#                                                                           #
# You should have received a copy of the GNU Lesser General Public License  #
# along with pysgpp; if not, write to the Free Software                     #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA #
# or see <http://www.gnu.org/licenses/>.                                    #
#############################################################################


import unittest

class TestSGridIndex(unittest.TestCase):
    def testConstructor(self):
        """Tests copy constructor"""
        from pysgpp import GridIndex
        
        s = GridIndex(2)
        s.set(0,1,1)
        s.set(0,2,3)
        
        s2 = GridIndex(s)
        self.failUnlessEqual(s.get(0), s2.get(0))
        
    def testAssign(self):
        """Tests assignment"""
        from pysgpp import GridIndex
        
        s = GridIndex(2)
        s.set(0,1,1)
        s.set(1,2,3)
        
        s2 = GridIndex(5)
        
        s2.assign(s)
        self.failUnlessEqual(s.get(0), s2.get(0))
        self.failUnlessEqual(s.get(1), s2.get(1))
        