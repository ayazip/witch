"""
BenchExec is a framework for reliable benchmarking.
This file is part of BenchExec.

Copyright (C) 2007-2015  Dirk Beyer
Copyright (C) 2016-2019  Marek Chalupa
All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

import re
from os.path import join, splitext, dirname
from symbiotic.witnesses.YAMLwitnesswriter import YAMLWriter

try:
    import benchexec.util as util
    import benchexec.result as result
except ImportError:
    # fall-back solution (at least for now)
    import symbiotic.benchexec.util as util
    import symbiotic.benchexec.result as result

from symbiotic.exceptions import SymbioticException
from .. utils.utils import print_stdout, print_stderr, process_grep, err
from . kleebase import SymbioticTool as KleeBase
from . kleebase import get_ktest

class SymbioticTool(KleeBase):
    """
    Symbiotic tool info object
    """

    def __init__(self, opts):
        KleeBase.__init__(self, opts)
        self._generate_witness = False
        self.shifted = {}
        self.og_file = None

        self._patterns = [
            ('ESTPTIMEOUT', re.compile('.*query timed out (resolve).*')),
            ('EKLEETIMEOUT', re.compile('.*HaltTimer invoked.*')),
            ('EEXTENCALL', re.compile('.*failed external call.*')),
            ('EEXTENCALLDIS', re.compile('.*external calls disallowed.*')),
            ('ELOADSYM', re.compile('.*ERROR: unable to load symbol.*')),
            ('EINVALINST', re.compile('.*LLVM ERROR: Code generator does not support.*')),
            ('EINITVALS', re.compile('.*unable to compute initial values.*')),
            ('ESYMSOL', re.compile('.*unable to get symbolic solution.*')),
            ('ESILENTLYCONCRETIZED', re.compile('.*silently concretizing.*')),
            ('EEXTRAARGS', re.compile('.*calling .* with extra arguments.*')),
            ('EMALLOC', re.compile('.*found huge malloc, returning 0.*')),
            ('ESKIPFORK', re.compile('.*skipping fork.*')),
            ('EKILLSTATE', re.compile('.*killing.*states \(over memory cap\).*')),
            ('EPTHREAD', re.compile('.*ERROR:.*Call to pthread_.*')),
            ('EPTHREAD2', re.compile('.*ERROR:.*unsupported pthread API.*')),
            ('EMAKESYMBOLIC', re.compile(
                '.*memory error: invalid pointer: make_symbolic.*')),
            ('EVECTORUNSUP', re.compile('.*XXX vector instructions unhandled.*')),
            ('EASM', re.compile('.*ERROR:.*inline assembly is unsupported.*')),
            ('EMEMALLOC', re.compile('.*KLEE: WARNING: Allocating memory failed.*')),
            ('ESTACKOVFLW', re.compile('.*WARNING: Maximum stack size reached.*')),
            ('EROSYMB', re.compile('.*cannot make readonly object symbolic.*')),
            ('EFUNMODEL', re.compile('.*: unsupported function model.*')),
            ('ERESOLVMEMCLN', re.compile('.*Failed resolving segment in memcleanup check.*')),
            ('ERESOLVMEMCLN2', re.compile('.*Cannot resolve non-constant segment in memcleanup check.*')),
            ('ECMP', re.compile('.*Comparison other than (in)equality is not implemented.*')),
            ('ERESOLV', re.compile('.*Failed resolving.*segment.*')),
            ('EUNREACH', re.compile('.*reached "unreachable" instruction.*')),
            ('ERESOLV', re.compile('.*ERROR:.*Could not resolve.*'))
        ]

    def executable(self):
        """
        Find the path to the executable file that will get executed.
        This method always needs to be overridden,
        and most implementations will look similar to this one.
        The path returned should be relative to the current directory.
        """
        return util.find_executable('witch-klee')

    def name(self):
        """
        Return the name of the tool, formatted for humans.
        """
        return 'witch-klee'

    def cmdline(self, executable, options, tasks, propertyfile=None, rlimits={}):
        """
        Compose the command line to execute from the name of the executable
        """

        opts = self._options
        prop = opts.property

        cmd = [executable] + self._arguments

        if opts.timeout is not None:
               cmd.append('-max-time={0}'.format(opts.timeout))

        if prop.memsafety():
            if opts.sv_comp:
                cmd.append('-check-leaks')
            else: # if not in SV-COMP, consider any unfreed memory as a leak
                cmd.append('-check-memcleanup')
        elif prop.memcleanup():
            cmd.append('-check-memcleanup')

        # filter out the non-standard error calls,
        # because we support only one such call atm.
        if prop.unreachcall():
            calls = [x for x in prop.getcalls() if x not in ['__VERIFIER_error', '__assert_fail']]
            if calls:
                assert len(calls) == 1, "Multiple error functions unsupported yet"
                cmd.append('-error-fn={0}'.format(calls[0]))
            # FIXME: append to all properties?
            cmd.append('-malloc-symbolic-contents')

        if opts.exit_on_error:
            print("Witch-KLEE does not support -exit-on-error")

        if self._options.witness_check_file is None:
            raise SymbioticException("Witch-KLEE needs a witness (--witness-check=<witness>)")

        if opts.guide_only:
            cmd.append('-guide-only=true')
            cmd.append('-write-waypoints')

        return cmd + options + tasks + self._options.argv + [self._options.witness_check_file]

    def determine_result(self, returncode, returnsignal, output, isTimeout):
        opts = self._options
        prop = opts.property

        if isTimeout:
            return 'timeout'

        if output is None:
            return 'ERROR (no output)'

        parsing_failed = None
        unknown = False
        for line in output:
            if b'Parsing failed' in line:
                parsing_failed = line.strip().split(b':')[-1].strip().decode('utf-8')

            if b'Error found when using the witness as a guide' in line:
                print_stdout("Error found before witness was confirmed.")

                if self._options.witness_output:
                    print_stdout("Generating a new witness.")

                self._generate_witness = True

            if b'Valid violation witness' in line or b'Error found when using the witness as a guide' in line:
                if b'unreach-call' in line:
                    return result.RESULT_FALSE_REACH
                if b'valid-free' in line:
                    return result.RESULT_FALSE_FREE
                if b'valid-deref' in line:
                    return result.RESULT_FALSE_DEREF
                if b'valid-memtrack' in line:
                    return result.RESULT_FALSE_MEMTRACK
                if b'valid-memcleanup' in line:
                    return result.RESULT_FALSE_MEMCLEANUP
                if b'no-overflow' in line:
                    return result.RESULT_FALSE_OVERFLOW
                if b'termination' in line:
                    return result.RESULT_FALSE_TERMINATION

            if b'may not be confirmed' in line:
                unknown = True
            if b'Follow waypoint of segment' in line:
                print_stdout(line.decode('utf-8'))
        if returncode != 0:
            if parsing_failed:
                return f'{result.RESULT_ERROR} ({parsing_failed})'
            return f'{result.RESULT_ERROR} (exitcode {returncode})'

        for line in output:
            for (key, pattern) in self._patterns:
                if pattern.match(str(line)):
                    return "{0} ({1})".format(result.RESULT_UNKNOWN, " ".join(key))

        return result.RESULT_UNKNOWN if unknown else result.RESULT_TRUE_PROP


    def set_environment(self, env, opts):
        """
        Set environment for the tool
        """
        if opts.devel_mode:
            env.prepend('PATH', '{0}/witch-klee/build-{1}/bin'.\
                        format(env.symbiotic_dir, self.llvm_version()))
            # XXX: we must take the runtime libraries from the install directory
            # because we have them compiled for 32-bit and 64-bit separately
            #(in build, there's only one of them)
            prefix = '{0}/install'.format(env.symbiotic_dir)
        else:
            prefix = '{0}'.format(env.symbiotic_dir)
            env.prepend('PATH', '{0}/llvm-{1}/witch-klee/bin'.format(env.symbiotic_dir, self.llvm_version()))
            env.prepend('LD_LIBRARY_PATH', '{0}/llvm-{1}/witch-klee/lib'.format(env.symbiotic_dir, self.llvm_version()))

        if opts.is32bit:
            env.prepend('KLEE_RUNTIME_LIBRARY_PATH',
                         '{0}/llvm-{1}/witch-klee/lib32/klee/runtime'.\
                         format(prefix, self.llvm_version()))
        else:
            env.prepend('KLEE_RUNTIME_LIBRARY_PATH',
                        '{0}/llvm-{1}/witch-klee/lib/klee/runtime'.\
                        format(prefix, self.llvm_version()))

    def passes_after_slicing(self):
        passes = []

        # make the uninitialized variables symbolic (if desired)
        if not self._options.explicit_symbolic:
            passes.append('-initialize-uninitialized')

        # make external globals non-deterministic
        if not self._options.sv_comp:
            passes.append('-internalize-globals')

        # for the memsafety property, make functions behave like they have
        # side-effects, because LLVM optimizations could remove them otherwise,
        # even though they contain calls to assert
        if self._options.property.memsafety():
            passes.append('-remove-readonly-attr')

        elif self._options.property.termination():
            passes.append('-witch-instrument-nontermination')
            passes.append('-instrument-nontermination-mark-header')

        return passes

    def slicer_options(self):
        """
        Returns tuple (c, opts) where c is a list with slicing
        criteria and opts is a list of options
        """

        assert self._options.property.unreachcall(), 'Slicing only supported for unreach-call'
        return self._options.property.getcalls(), ['-2c', '__VALIDATOR_branch,__VALIDATOR_switch,__VALIDATOR_assume,__VALIDATOR_segment']

        return ([],[])

    def generate_witness(self, llvmfile, sources, has_error):
        saveto = self._options.witness_output
        if not self._generate_witness or not has_error:
            return

        assert len(sources) == 1 and "Can not generate witnesses for more sources yet"
        print('Generating YAML witness: {0}'.format(saveto))

        if self._options.property.memcleanup():
            print('Failed generating YAML witness: Property not supported by format')
            return

        pth = get_ktest(join(dirname(llvmfile), 'klee-last'))
        test = '{0}.waypoints'.format(splitext(pth)[0])

        assert saveto is not None
        gen = YAMLWriter(self.og_file, self._options.property,
                         self._options.is32bit, not has_error, saveto, self.shifted)
        gen.generate_violation_witness(test)
        gen.write(saveto)
