#
#
#

import sys
import os
import unittest

# Dynamically append current pykd.pyd path to PYTHONPATH
sys.path.insert(0, os.path.dirname(sys.argv[1]))

import pykd

import target

import intbase
import memtest
import moduletest
import typeinfo
import typedvar
import regtest
import mspdbtest
import localstest
import customtypestest
import ehexcepttest
import ehstatustest
import ehsymbolstest

class StartProcessWithoutParamsTest(unittest.TestCase):
    def testStart(self):
        target.processId = pykd.startProcess( target.appPath )
        target.module = pykd.module( target.moduleName )
        target.module.reload();
        print "\n" + str( pykd.getSystemVersion() )
        pykd.go()

class TerminateProcessTest(unittest.TestCase):
    def testKill(self):
        pykd.killProcess( target.processId )

def getTestSuite( singleName = "" ):
    if singleName == "":
        return unittest.TestSuite(
           [
                unittest.TestLoader().loadTestsFromTestCase( StartProcessWithoutParamsTest ),
                # *** Test without start/kill new processes
                unittest.TestLoader().loadTestsFromTestCase( intbase.IntBaseTest ),
                unittest.TestLoader().loadTestsFromTestCase( moduletest.ModuleTest ),
                unittest.TestLoader().loadTestsFromTestCase( memtest.MemoryTest ),
                unittest.TestLoader().loadTestsFromTestCase( typeinfo.TypeInfoTest ),
                unittest.TestLoader().loadTestsFromTestCase( typedvar.TypedVarTest ),
                unittest.TestLoader().loadTestsFromTestCase( regtest.CpuRegTest ),
                unittest.TestLoader().loadTestsFromTestCase( customtypestest.CustomTypesTest ),
                # ^^^
                unittest.TestLoader().loadTestsFromTestCase( TerminateProcessTest ),

                unittest.TestLoader().loadTestsFromTestCase( mspdbtest.MsPdbTest ),
                unittest.TestLoader().loadTestsFromTestCase( localstest.LocalVarsTest ),
                unittest.TestLoader().loadTestsFromTestCase( ehexcepttest.EhExceptionTest ),
                unittest.TestLoader().loadTestsFromTestCase( ehstatustest.EhStatusTest ),
                unittest.TestLoader().loadTestsFromTestCase( ehsymbolstest.EhSymbolsTest ),
            ] ) 
    else:
       return unittest.TestSuite(
          [
                unittest.TestLoader().loadTestsFromTestCase( StartProcessWithoutParamsTest ),
                unittest.TestLoader().loadTestsFromName( singleName ),
                unittest.TestLoader().loadTestsFromTestCase( TerminateProcessTest )
          ] )
          
if __name__ == "__main__":

    print "\nTesting PyKd ver. " + pykd.__version__

    target.appPath = sys.argv[1]
    target.moduleName = os.path.splitext(os.path.basename(target.appPath))[0]
    
    unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run( getTestSuite() )


