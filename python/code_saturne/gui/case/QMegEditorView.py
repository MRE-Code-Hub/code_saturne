# -*- coding: utf-8 -*-

#-------------------------------------------------------------------------------

# This file is part of code_saturne, a general-purpose CFD tool.
#
# Copyright (C) 1998-2024 EDF S.A.
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
# Street, Fifth Floor, Boston, MA 02110-1301, USA.

#-------------------------------------------------------------------------------

"""
This module defines the Dialog window of the mathematical expression generator.
Its is based on the MEI tools

This module contains the following classes and function:
- format
- QMegHighlighter
- QMegEditorView
"""

#-------------------------------------------------------------------------------
# Library modules import
#-------------------------------------------------------------------------------

import os
import sys
import logging

#-------------------------------------------------------------------------------
# Third-party modules
#-------------------------------------------------------------------------------

from code_saturne.gui.base.QtCore    import *
from code_saturne.gui.base.QtGui     import *
from code_saturne.gui.base.QtWidgets import *
from code_saturne.gui.base.QtPage import ComboModel
from code_saturne.gui.base.CompletionTextEditor import *

#-------------------------------------------------------------------------------
# Application modules import
#-------------------------------------------------------------------------------

from code_saturne.base.cs_meg_to_c import meg_to_c_interpreter
from code_saturne.model.Common import GuiParam
from code_saturne.gui.case.QMegEditorForm import Ui_QMegDialog

#-------------------------------------------------------------------------------
# log config
#-------------------------------------------------------------------------------

logging.basicConfig()
log = logging.getLogger("QMegEditorView")
log.setLevel(GuiParam.DEBUG)

#-------------------------------------------------------------------------------
# Syntax highlighter for the mathematical expressions editor.
#-------------------------------------------------------------------------------

def format(color, style=''):
    """Return a QTextCharFormat with the given attributes."""
    _color = QColor()
    _color.setNamedColor(color)

    _format = QTextCharFormat()
    _format.setForeground(_color)
    if 'bold' in style:
        _format.setFontWeight(QFont.Bold)
    if 'italic' in style:
        _format.setFontItalic(True)

    return _format


   # Syntax styles that can be shared by all expressions
STYLES = {
    'keyword': format('blue', 'bold'),
    'operator': format('red'),
    'brace': format('darkGray'),
    'required': format('magenta', 'bold'),
    'symbols': format('darkMagenta', 'bold'),
    'comment': format('darkGreen', 'italic'),
    }


class QMegHighlighter(QSyntaxHighlighter):
    """
    Syntax highlighter for the mathematical expressions editor.
    """
    keywords = [
         'cos', 'sin', 'tan', 'exp', 'sqrt', 'log',
         'acos', 'asin', 'atan', 'atan2', 'cosh', 'sinh',
         'tanh', 'abs', 'mod', 'int', 'min', 'max',
         'square_norm',
         'pi', 'e', 'while', 'if', 'else', 'FOR_RANGE'
    ]

    operators = [
        # logical
        '!', '==', '!=', '<', '<=', '>', '>=', '&&', '\|\|',
        # Arithmetic
        '=', '\+', '-', '\*', '/', '\^',
    ]

    braces = ['\{', '\}', '\(', '\)', '\[', '\]',]


    def __init__(self, document, required, symbols):
        QSyntaxHighlighter.__init__(self, document)

        rules = []
        rules += [(r'\b%s\b' % w, STYLES['keyword']) for w in QMegHighlighter.keywords]
        rules += [(r'%s' % o, STYLES['operator']) for o in QMegHighlighter.operators]
        rules += [(r'%s' % b, STYLES['brace']) for b in QMegHighlighter.braces]
        rules += [(r'\b%s\b' % r, STYLES['required']) for r,l in required]
        rules += [(r'\b%s\b' % s, STYLES['symbols']) for s,l in symbols]
        rules += [(r'#[^\n]*', STYLES['comment'])]

        # Build a QRegExp for each pattern
        self.rules = [(QRegExp(pat), fmt) for (pat, fmt) in rules]


    def highlightBlock(self, text):
        """
        Apply syntax highlighting to the given block of text.
        """
        for rx, fmt in self.rules:
            pos = rx.indexIn(text, 0)
            while pos != -1:
                pos = rx.pos(0)
                s = rx.cap(0)
                self.setFormat(pos, len(s), fmt)
                pos = rx.indexIn( text, pos+rx.matchedLength() )

#-------------------------------------------------------------------------------
# Dialog for mathematical expression interpretor
#-------------------------------------------------------------------------------

class QMegEditorView(QDialog, Ui_QMegDialog):
    """
    """
    def __init__(self, parent, function_type, zone_name, variable_name,
                expression = '', required = [], symbols = [], known_fields = [],
                condition = None, source_type = None, examples = ""):
        """
        Constructor.
        """
        QDialog.__init__(self, parent)

        Ui_QMegDialog.__init__(self)
        self.setupUi(self)

        self.meg_to_c = meg_to_c_interpreter(parent.case, False)

        self.meg_to_c.init_block(function_type,
                                 zone_name,
                                 variable_name,
                                 expression,
                                 required,
                                 symbols,
                                 known_fields,
                                 condition,
                                 source_type)

        sym = [('todelete','')]
        base = []
        for s,l in symbols:
            if s.find("[") != -1:
                idx = s.find("[")
                if s[:idx] not in base:
                    sym.append((s[:idx], l))
                    base.append(s[:idx])
            else:
                sym.append((s,l))
        del sym[0]

        req = [('todelete','')]
        base = []
        for s,l in required:
            if s.find("[") != -1:
                idx = s.find("[")
                if s[:idx] not in base:
                    req.append((s[:idx], l))
                    base.append(s[:idx])
            else:
                req.append((s,l))
        del req[0]

        self.required = required
        self.symbols  = symbols

        # Syntax highlighting
        CompletionTextEdit(self.textEditExpression)
        self.h1 = QMegHighlighter(self.textEditExpression, req, sym)
        self.h2 = QMegHighlighter(self.textEditExamples, req, sym)

        # Required symbols of the mathematical expression

        if not expression:
            expression = ""
            for p, q in required:
                expression += p + " = \n"
        else:
            while expression[0] in (" ", "\n", "\t"):
                expression = expression[1:]
            while expression[-1] in (" ", "\n", "\t"):
                expression = expression[:-1]

        # Predefined symbols for the mathematical expression

        predif = self.tr("<big><u>Required symbol:</u></big><br>")

        for p,q in required:
            for s in ("<b>", p, "</b>: ", q, "<br>"):
                predif += s

        if symbols:
            predif += self.tr("<br><big><u>Predefined symbols:</u></big><br>")

            for p,q in symbols:
                for s in ("<b>", p, "</b>: ", q, "<br>"):
                    predif += s

        predif += self.tr("<br>"\
                  "<big><u>Useful functions:</u></big><br>"\
                  "<b>cos</b>: cosine<br>"\
                  "<b>sin</b>: sine<br>"\
                  "<b>tan</b>: tangent<br>"\
                  "<b>exp</b>: exponential<br>"\
                  "<b>sqrt</b>: square root<br>"\
                  "<b>log</b>: napierian logarithm<br>"\
                  "<b>acos</b>: arc cosine<br>"\
                  "<b>asin</b>: arcsine<br>"\
                  "<b>atan</b>: arc tangent<br>"\
                  "<b>atan2</b>: arc tangent (two variables)<br>"\
                  "<b>cosh</b>: hyperbolic cosine<br>"\
                  "<b>sinh</b>: hyperbolic sine<br>"\
                  "<b>tanh</b>: hyperbolic tangent<br>"\
                  "<b>abs</b>: absolute value<br>"\
                  "<b>mod</b>: modulo<br>"\
                  "<b>int</b>: floor<br>"\
                  "<b>min</b>: minimum<br>"\
                  "<b>max</b>: maximum<br>"\
                  "<b>square_norm</b>: square norm of a vector<br>"\
                  "<br>"\
                  "<big><u>Useful constants:</u></big><br>"\
                  "<b>pi</b> = 3.14159265358979323846<br>"\
                  "<b>e</b> = 2.718281828459045235<br>"\
                  "<br>"\
                  "<big><u>Operators and statements:</u></big><br>"\
                  "<b><code> + - * / ^ </code></b><br>"\
                  "<b><code>! &lt; &gt; &lt;= &gt;= == != && || </code></b><br>"\
                  "<b><code>while if else</code></b><br>"\
                  "<b><code>FOR_RANGE(var_name,var_start,var_stop)</code></b><br>"\
                  "")

        # lay out the text

        self.textEditExpression.setText(expression)
        self.textEditSymbols.setText(predif)
        self.textEditExamples.setText(examples)

        # Autocompletion test:
        completer = QCompleter(parent=self.textEditExpression)
        self.textEditExpression.setCompleter(completer)

        qacm = QStringListModel()
        self.textEditExpression.completer.setModel(qacm)
        ll = []
        for p, q in required:
            ll.append(p)
        if symbols:
            for p, q in symbols:
                ll.append(p)
        qacm.setStringList(ll)

        self.expressionDoc = self.textEditExpression.document()

        # ------------------
        # Calculator Buttons
        # ------------------

        # numbers
        for i in range(10):
            _button = getattr(self, "pushButton_" + str(i))
            _button.clicked.connect(lambda state, v=i: self._addOperator(str(v)))

        # functions :
        l_ = ['exp', 'sqrt', 'log', 'abs', 'cos', 'sin', 'tan', 'acos', 'asin', \
              'atan', 'sinh', 'cosh', 'tanh', 'min', 'max']
        for o_ in l_:
            _button = getattr(self, "pushButton_" + o_)
            _button.clicked.connect(lambda state, v=" {}() ".format(o_) : self._addOperator(v))

        # operators
        self.pushButton_plus.clicked.connect(lambda :self._addOperator(" + "))
        self.pushButton_minus.clicked.connect(lambda :self._addOperator(" - "))
        self.pushButton_mult.clicked.connect(lambda :self._addOperator(" * "))
        self.pushButton_divide.clicked.connect(lambda :self._addOperator(" / "))
        self.pushButton_eq.clicked.connect(lambda :self._addOperator(" = "))
        self.pushButton_dot.clicked.connect(lambda :self._addOperator("."))
        self.pushButton_comma.clicked.connect(lambda :self._addOperator(", "))
        self.pushButton_semi_col.clicked.connect(lambda :self._addOperator(";\n"))

        self.pushButton_lt.clicked.connect(lambda :self._addOperator(" < "))
        self.pushButton_gt.clicked.connect(lambda :self._addOperator(" > "))
        self.pushButton_leq.clicked.connect(lambda :self._addOperator(" <= "))
        self.pushButton_geq.clicked.connect(lambda :self._addOperator(" >= "))

        self.pushButton_par_open.clicked.connect(lambda :self._addOperator("("))
        self.pushButton_par_close.clicked.connect(lambda :self._addOperator(")"))
        self.pushButton_curl_open.clicked.connect(lambda :self._addOperator("{"))
        self.pushButton_curl_close.clicked.connect(lambda :self._addOperator("}"))

        self.pushButton_power.clicked.connect(lambda :self._addOperator("^"))
        self.pushButton_pi.clicked.connect(lambda :self._addOperator("pi"))

        self.pushButton_if.clicked.connect(lambda :self._addOperator("if ()"))

        # ComboModel for known symbols
        self.known_symbols_box = ComboModel(self.comboBox_Insert, 1, 1)
        for p, q in symbols:
            self.known_symbols_box.addItem(self.tr(p,p))

        self.comboBox_Insert.activated[str].connect(self._addOperator)

    @pyqtSlot()
    def slotClearBackground(self):
        """
        Private slot.
        """
        self.textEditExpression.disconnect()
        doc = self.textEditExpression.document()

        for i in range(0, doc.blockCount()):
            block_text = doc.findBlockByNumber(i)
            log.debug("block: %s" % block_text.text())
            cursor = QTextCursor(block_text)
            block_format = QTextBlockFormat()
            block_format.clearBackground()
            cursor.setBlockFormat(block_format)

    @pyqtSlot(str)
    def _addOperator(self, _operator):
        self.textEditExpression.textCursor().insertText(_operator)

    def accept(self):
        """
        What to do when user clicks on 'OK'.
        """

        if self.meg_to_c is None:
            QDialog.accept(self)
            return

        log.debug("accept()")

        doc = self.textEditExpression.document()

        log.debug("check.string: %s" % str(self.textEditExpression.toPlainText()))

        # The provided meg_to_c interpreter should have only one block.
        # Could, and should, be modified in the future to identify
        # the good key if needed...
        new_exp = str(self.textEditExpression.toPlainText()) + '\n'
        check = 0
        for func_type in self.meg_to_c.funcs.keys():
            for k in self.meg_to_c.funcs[func_type].keys():
                self.meg_to_c.update_block_expression(func_type, k, new_exp)
                check, err_msg, n_erros = self.meg_to_c.check_meg_code_syntax(func_type)

        if check != 0:
            log.debug(err_msg)
            QMessageBox.critical(self, self.tr('Expression Editor'), err_msg)
            self.textEditExpression.textChanged.connect(self.slotClearBackground)
            return

        self.meg_to_c.clean_tmp_dir()

        QDialog.accept(self)


    def reject(self):
        """
        Method called when 'Cancel' button is clicked
        """
        self.meg_to_c.clean_tmp_dir()
        log.debug("reject()")
        QDialog.reject(self)


    def get_result(self):
        """
        Method to get the result
        """
        return self.textEditExpression.toPlainText()


#-------------------------------------------------------------------------------
# Test function
#-------------------------------------------------------------------------------

if __name__ == "__main__":
    import sys, signal
    app = QApplication(sys.argv)
    app.lastWindowClosed.connect(app.quit)
    parent = QWidget()
    dlg = QMegEditorView(parent)
    dlg.show()
    signal.signal(signal.SIGINT, signal.SIG_DFL)
    sys.exit(app.exec_())

#-------------------------------------------------------------------------------
# End
#-------------------------------------------------------------------------------
