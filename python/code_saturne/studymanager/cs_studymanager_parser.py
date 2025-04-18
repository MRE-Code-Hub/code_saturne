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

#-------------------------------------------------------------------------------
# Standard modules import
#-------------------------------------------------------------------------------

import os, sys, logging, re
from xml.dom import minidom
from xml.sax.handler import ContentHandler
from xml.sax import make_parser

#-------------------------------------------------------------------------------
# log config
#-------------------------------------------------------------------------------

logging.basicConfig()
log = logging.getLogger(__file__)
log.setLevel(logging.NOTSET)
#log.setLevel(logging.DEBUG)

#-------------------------------------------------------------------------------
# Checker of XML file syntax
#-------------------------------------------------------------------------------

def xmlChecker(filename):
    """Try to open the xml file, and return a message if an error occurs.

    @param filename name of the file of parameters ith its absolute path
    @return m error message
    """
    m = ""

    try:
        p = make_parser()
        p.setContentHandler(ContentHandler())
        p.parse(filename)
    except Exception as e:
        f = os.path.basename(filename)
        m = "%s file reading error. \n\n"\
            "This file is not in accordance with XML specifications.\n\n"\
            "The parsing syntax error is:\n\n%s" % (f, e)

    return m

#-------------------------------------------------------------------------------
# Reader of the XML file
#-------------------------------------------------------------------------------

class Parser(object):
    """ Parser -- class to parse XML file."""

    def __init__ (self, XMLFileName, doc=None):
        """
        Constructor of the XML reader.
        @type XMLFileName: C{String}
        @param XMLFileName: name of the xml file
        """
        self.filename = XMLFileName
        self.__repo = None
        self.__dest = None

        if doc != None:
            self.doc = doc
        else:
            try:
                self.doc =  minidom.parse(XMLFileName)
            except:
                print("Error in the syntax of the xml.\n")
                msg =  xmlChecker(self.filename)
                if msg:
                    print(msg)
                sys.exit(1)

        self.root = self.doc.firstChild

        if self.root.nodeName != "studymanager":
            print(XMLFileName + ": not a studymanager XML file.")
            sys.exit(1)

    #---------------------------------------------------------------------------

    def write(self):
        return self.doc.toxml()

    #---------------------------------------------------------------------------

    def getDataFromNode(self, father, childName):
        """
        @type father: C{DOM element}
        @param father: father node
        @type childName: C{String}
        @param childName: label of a markup, child of the father node
        @rtype: C{String}
        @return: data from the markup I{childName}
        """
        data = None
        l = father.getElementsByTagName(childName)

        if len(l) == 1:
            current = l.item(0)
            if current.firstChild:
                data = current.firstChild.data
            else:
                data = None
        elif len(l) == 0:
            print("Error: in getDataFromNode no markup %s found." %  childName)
            sys.exit(1)
        else:
            print("Error: in getDataFromNode several markups %s found." %  childName)
            sys.exit(1)

        return data

    #---------------------------------------------------------------------------

    def addChild(self, father, childname):
        """
        Add a new node as a child of the father node.
        @type father: C{DOM element}
        @param father: father node
        @type childName: C{String}
        @param childName: label of a markup, child of the father node
        @rtype: C{DOM element}
        @return: new node I{childName}
        """
        return father.appendChild(self.doc.createElement(childname))

    #---------------------------------------------------------------------------

    def getChild(self, father, childname):
        """
        Return a node as a child of the father node.
        @type father: C{DOM element}
        @param father: father node
        @type childName: C{String}
        @param childName: label of a markup, child of the father node
        @rtype: C{DOM element}
        @return: new node I{childName}
        """
        l = father.getElementsByTagName(childname)

        if len(l) == 0:
            return None
        elif len(l) == 1:
            return l.item(0)
        else:
            print("Error: in getChild several markup %s found." %  childname)
            sys.exit(1)

    #---------------------------------------------------------------------------

    def getChildren(self, father, childname):
        """
        Return a list of child of the father node.
        @type father: C{DOM element}
        @param father: father node
        @type childName: C{String}
        @param childName: label of a markup, childs of the father node
        @rtype: C{List} of C{DOM element}
        @return: list of nodes I{childName}
        """
        return father.getElementsByTagName(childname)

    #---------------------------------------------------------------------------

    def getRepository(self):
        """
        @rtype: C{String}
        @return: repository directory of all studies.
        """
        if self.__repo is None:
            self.__repo = self.getDataFromNode(self.root, "repository")

        return self.__repo;

    #---------------------------------------------------------------------------

    def setRepository(self, repo_path):
        """
        @rtype: C{String}
        @param: repository directory of all studies.
        """

        self.__repo = os.path.expanduser(repo_path);
        self.__repo = os.path.abspath(self.__repo);

    #---------------------------------------------------------------------------

    def getDestination(self):
        """
        @rtype: C{String}
        @return: destination directory of all studies.
        """
        if self.__dest is None:
            self.__dest = self.getDataFromNode(self.root, "destination")

        return self.__dest

    #---------------------------------------------------------------------------

    def setDestination(self, dest_path):
        """
        @rtype: C{String}
        @param: destination directory of all studies.
        """
        self.__dest = os.path.expanduser(dest_path);
        self.__dest = os.path.abspath(self.__dest);

    #---------------------------------------------------------------------------

    def getStudiesLabel(self):
        """
        Read:
            <study label='STUDY' status='on'>
                <case label='CASE' status='on'/>
            </study>

        @rtype: C{List} of C{String}
        @return: list of the label of all studies.
        """
        labels = []

        for node in self.root.getElementsByTagName("study"):
            label  = str(node.attributes["label"].value)
            status = str(node.attributes["status"].value)
            if status == 'on':
                if label not in labels:
                    labels.append(label)
                else:
                    print("Several occurences of Study %s in xml file of parameters" % label)
                    sys.exit(1)
        return labels

    #---------------------------------------------------------------------------

    def getStudyNode(self, l):
        """
        Read:
            <study label='STUDY' status='on'>
                <case label='CASE' status='on'/>
            </study>

        @type l: C{String}
        @param l: label of a study
        @rtype: C{DOM element}
        @return: node of the xml document for study I{l}
        """
        node = None

        for n in self.root.getElementsByTagName("study"):
            label = str(n.attributes["label"].value)
            if label == l:
                if str(n.attributes["status"].value) != "on":
                    raise ValueError("Error: the getStudyNode method is used with the study %s turned off " % l)
                node = n
                break

            if not n:
                raise ValueError("Error: the getStudyNode does not found the node of the study %s" % l)

        return node

    #---------------------------------------------------------------------------

    def getStudyTags(self, studyNode):
        """
        Read:
            <study label='STUDY' status='on' tags='coarse, test'>
                <case label='CASE' status='on'/>
            </study>

        @type studyNode: C{DOM Element}
        @param studyNode: node of the current study
        @rtype: C{List} of C{String}
        @return: list of the tags of a study
        """
        if str(studyNode.attributes["status"].value) != "on":
            raise ValueError("Error: the getStudyTags method is used with the "\
                             "study %s turned off " % l)

        try:
            text = str(studyNode.attributes["tags"].value)
            tags = [tag.strip() for tag in re.split(',', text)]
        except:
            tags = []

        return tags

    #---------------------------------------------------------------------------

    def getStudyKeywords(self, studyNode):
        """
        Read:
            <study label='STUDY' status='on' keywords='laminar, 2D'>
                <case label='CASE' status='on'/>
            </study>

        @type studyNode: C{DOM Element}
        @param studyNode: node of the current study
        @rtype: C{List} of C{String}
        @return: list of the keywords of a study
        """
        keywords = []
        for node_kw in studyNode.getElementsByTagName("study_keywords"):
            try:
                text = str(node_kw.firstChild.data)
                keywords.extend([kw.strip() for kw in re.split(',', text)])
            except:
                continue

        return keywords

    #---------------------------------------------------------------------------

    def getStatusOnCasesLabels(self, l, attr=None):
        """
        Read:
            <study label='STUDY' status='on'>
                <case label='CASE' status='on'/>
            </study>

        @type l: C{String}
        @param l: label of a study
        @type attr: C{String}
        @param attr: attribute I{run_id} or I{tags} or I{other}
        @rtype: C{List} of C{String}
        @return: list of cases for study I{l}
        """
        labels = []

        for node in self.getStudyNode(l).getElementsByTagName("case"):
            status = str(node.attributes["status"].value)
            if status == 'on':
                labels.append(str(node.attributes["label"].value))

                if attr and str(node.attributes[attr].value) != 'on':
                    labels.pop()

        return labels

    #---------------------------------------------------------------------------

    def getStudyMetadata(self, l):
        """
        test
        """

        data = {"study":{}, "cases":[]}

        study_node = self.getStudyNode(l)

        # STUDY
        data['study']['name'] = str(l)
        data['study']['keywords'] = self.getStudyKeywords(study_node)
        data['study']['tags::study'] = self.getStudyTags(study_node)
        data['study']['tags::cases'] = []


        # First pass based on availability of case descriptions
        for node_descr in study_node.getElementsByTagName("case_description"):
            case_name = str(node_descr.attributes['label'].value)

            for c in data['cases']:
                if c['name'] == case_name:
                    raise Exception(f'More than one description for label {case_name} in file {filename}')

            new_d = {'name':None,
                     'tags':[],
                     'vnvitem':[]}

            new_d['name'] = case_name

            for n in node_descr.getElementsByTagName('item'):
                new_d['vnvitem'].append(str(n.firstChild.data))

            data['cases'].append(new_d)

        # Get tags from the different subcases
        for node in study_node.getElementsByTagName("case"):
            case_name = str(node.attributes['label'].value)

            case_id = -1
            for c_id, c in enumerate(data['cases']):
                if c['name'] == case_name:
                    case_id = c_id

            ctags = None
            try:
                tags = str(node.attributes["tags"].value)
                ctags = [tag.strip() for tag in re.split(',', tags)]
            except:
                ctags = []

            if case_id < 0:
                data['cases'].append({'name':case_name,
                                     'tags':[],
                                     'vnvitem':[]})

                case_id = len(data['cases']) - 1

            if ctags:
                data['cases'][case_id]['tags'] += ctags


        # Unique list of sub-cases tags for study
        tmp = []
        for c in data['cases']:
            tmp += c['tags']

        data['study']['tags::cases'] = list(dict.fromkeys(tmp))

        return data


    #---------------------------------------------------------------------------

    def getStatusOnCasesKeywords(self, l):
        """
        Read N_PROCS and USER_INPUT_FILES in:
            <study label='STUDY' status='on'>
                <case label='CASE1' run_id ="Grid 1" status='on'/>
                <case label='CASE2' status='on'/>
            </study>
        @type l: C{String}
        @param l: label of a study
        @rtype: C{Dictionary}
        @return: keywords and value.
        """
        data = []
        list_case = {}
        deprecated_option = False

        setup_filter_keys = ("notebook", "parametric", "kw_args")

        for node in self.getStudyNode(l).getElementsByTagName("case"):
            if str(node.attributes["status"].value) == 'on':

                d = {}
                d['node']  = node
                d['label'] = str(node.attributes["label"].value)

                # Deprecated compute, compare and post options
                try:
                    if node.attributes["compute"] or node.attributes["post"] or node.attributes["compare"]:
                        deprecated_option = True
                except:
                    # no deprecated option
                    pass

                # dictionary used to set missing run_id
                if d['label'] in list(list_case.keys()):
                    list_case[d['label']] += 1
                else:
                    list_case[d['label']] = 1

                try:
                    if node.attributes["run_id"].value:
                        d['run_id'] = str(node.attributes["run_id"].value)
                    else:
                        d['run_id'] = "run" + str(list_case[d['label']])
                except:
                    d['run_id'] = "run" + str(list_case[d['label']])

                try:
                    d['n_procs'] = str(node.attributes["n_procs"].value)
                except:
                    d['n_procs'] = None

                # tags defined for the case
                try:
                    tags = str(node.attributes["tags"].value)
                    d['tags'] = [tag.strip() for tag in re.split(',', tags)]
                except:
                    d['tags'] = []
                # add tags defined for the study
                d['tags'] += self.getStudyTags(self.getStudyNode(l))
                if len(d['tags']) == 0:
                    d['tags'] = None

                try:
                    depends = self.getDepends(node)
                    d['depends'] = str(depends)
                except:
                    d['depends'] = None

                # expected time used for slurm submissions
                try:
                    # convert in minutes data from case (format HH:MM)
                    tmp = str(node.attributes["expected_time"].value)
                    ind = tmp.find(":")
                    d['expected_time'] = int(tmp[:ind]) * 60 + int(tmp[-2:])
                except:
                    d['expected_time'] = None

                for k in setup_filter_keys:
                    d[k] = None

                for n in node.childNodes:
                    if n.nodeType == minidom.Node.ELEMENT_NODE:
                        if n.childNodes:
                            if n.tagName not in ("compare", "prepro", "script", "data", "depends"):
                                d[n.tagName] = n.childNodes[0].data
                        else:
                            if n.tagName in setup_filter_keys:
                                args = n.getAttribute("args")
                                if not d[n.tagName]:
                                    d[n.tagName] = str(args)
                                else:
                                    d[n.tagName] += " " + str(args)

                data.append(d)

        if deprecated_option:
            msg = "\n WARNING: deprecated options (compute, compare or post)" + \
                  " are found in the smgr xml file.\n Please update it with" + \
                  " --update-smgr.\n"
            print(msg)

        return data

    #---------------------------------------------------------------------------

    def getCompare(self, caseNode):
        """
        Read:
            <study label='STUDY' status='on'>
                <case label='CASE1' status='on'>
                    <compare repo="" dest="" args='--section Pressure --threshold 1e-2' status="on"/>
                </case>
            </study>
        @type caseNode: C{DOM Element}
        @param caseNode: node of the current case
        @rtype: C{True} or C{False}, C{String}, C{String}, C{Float}, C{String}
        @return: if the cs_io_dump/compare markup exists, and value of the threshold
        """
        compare, nodes, threshold, args, repo, dest = [], [], [], [], [], []

        for node in caseNode.getElementsByTagName("compare"):
            try:
                if str(node.attributes["status"].value) == 'on':
                    compare.append(True)
                else:
                    compare.append(False)
            except:
                compare.append(True)
            nodes.append(node)
            repo.append(str(node.attributes["repo"].value))
            dest.append(str(node.attributes["dest"].value))
            try:
                args.append(str(node.attributes["args"].value))
            except:
                args.append(None)
            try:
                threshold.append(str(node.attributes["threshold"].value))
            except:
                threshold.append(None)

        return compare, nodes, repo, dest, threshold, args

    #---------------------------------------------------------------------------

    def getPrepro(self, caseNode):
        """
        Read:
            <study label='STUDY' status='on'>
                <case label='CASE1' status='on'>
                    <prepro label="script_pre.py" args="" status="on"/>
                </case>
            </study>
        @type caseNode: C{DOM Element}
        @param caseNode: node of the current case
        """
        prepro, label, nodes, args = [], [], [], []

        for node in caseNode.getElementsByTagName("prepro"):
            if str(node.attributes["status"].value) == 'on':
                prepro.append(True)
            else:
                prepro.append(False)

            label.append(str(node.attributes["label"].value))
            nodes.append(node)
            try:
                args.append(str(node.attributes["args"].value))
            except:
                args.append("")

        return prepro, label, nodes, args

    #---------------------------------------------------------------------------

    def getScript(self, caseNode):
        """
        Read:
            <study label='STUDY' status='on'>
                <case label='CASE1' status='on'>
                    <script label="script_post.py" args="" dest="20110216-2147" status="on"/>
                </case>
            </study>
        @type caseNode: C{DOM Element}
        @param caseNode: node of the current case
        """
        script, label, nodes, args, repo, dest = [], [], [], [], [], []

        for node in caseNode.getElementsByTagName("script"):
            if str(node.attributes["status"].value) == 'on':
                script.append(True)
            else:
                script.append(False)

            label.append(str(node.attributes["label"].value))
            nodes.append(node)
            try:
                args.append(str(node.attributes["args"].value))
            except:
                args.append("")
            try:
                repo.append(str(node.attributes["repo"].value))
            except:
                repo.append(None)
            try:
                dest.append(str(node.attributes["dest"].value))
            except:
                dest.append(None)

        return script, label, nodes, args, repo, dest

    #---------------------------------------------------------------------------

    def getDepends(self, caseNode):
        """
        Read:
            <study label='STUDY' status='on'>
                <case label='CASE1' status='on'>
                    <depends args="STUDY/CASE/run_id"/>
                </case>
            </study>
        @type caseNode: C{DOM Element}
        @param caseNode: node of the current case
        """

        for node in caseNode.getElementsByTagName("depends"):
            args = str(node.attributes["args"].value)

        return args

    #---------------------------------------------------------------------------

    def getResult(self, node):
        """
        @type node: C{DOM Element}
        @param node: node of the current case
        @rtype: C{List}, C{List}
        @return: C{List} of nodes <resu>, and C{List} of file names
        """
        f  = str(node.attributes["file"].value)

        if node.tagName == "data":
            plots = node.getElementsByTagName("plot")
        else:
            plots = []

        try:
            dest  = str(node.attributes["dest"].value)
        except:
            dest = None
        try:
            repo = str(node.attributes["repo"].value)
        except:
            repo = None

        return plots, f, dest, repo

    #---------------------------------------------------------------------------

    def getInput(self, node):
        """
        @type node: C{DOM Element}
        @param node: node of the current case
        @rtype: C{List}, C{List}
        @return: C{List} of nodes <input>, and C{List} of file names
        """
        f  = str(node.attributes["file"].value)

        try:
            dest  = str(node.attributes["dest"].value)
        except:
            dest = None
        try:
            repo = str(node.attributes["repo"].value)
        except:
            repo = None
        try:
            tex = str(node.attributes["tex"].value)
        except:
            tex = None

        return  f, dest, repo, tex

    #---------------------------------------------------------------------------

    def getProbes(self, node):
        """
        Read:
            <probes file='monitoring_pressure.dat' dest="" fig="3"/>

        @type node: C{DOM Element}
        @param node: node of the current case
        """
        f  = str(node.attributes["file"].value)
        try:
            dest  = str(node.attributes["dest"].value)
        except:
            dest = None
        try:
            fig  = str(node.attributes["fig"].value)
        except:
            fig = None

        return f, dest, fig

    #---------------------------------------------------------------------------

    def getMeasurement(self, l):
        """
        Return the list of files of measurements.
        @type l: C{String}
        @param l: label of a study
        @rtype: C{List}
        @return: C{List} of list of nodes <plot>, and C{List} of files of measurements
        """
        nodes = []
        files = []

        for node in self.getStudyNode(l).getElementsByTagName("measurement"):
            nodes.append(node.getElementsByTagName("plot"))
            fileName = node.attributes["file"].value
            filePath = node.attributes["path"].value

            if filePath == "":
              for root, dirs, fs in os.walk(os.path.join(self.getRepository(), l)):
                  if fileName in fs:
                      filePath = root
                      break
            else: # for code_saturne exp data are supposed to be in POST
              for root, dirs, fs in os.walk(os.path.join(self.getRepository(), l, 'POST', filePath)):
                  if fileName in fs:
                      filePath = root
                      break

            files.append(os.path.join(filePath, fileName))

        return nodes, files

    #---------------------------------------------------------------------------

    def getPostPro(self, l):
        """
        Read:
            <study label='STUDY' status='on'>
                <case label='CASE1' status='on'/>
                <postpro label="script_post.py" args="" status="on">
                    <data file="profile.dat">
                        <plot fig="1" xcol="1" ycol="2" legend="Grid 1"/>
                    </data>
                </postpro>
            </study>

        Return the list of files of postpro.
        @type l: C{String}
        @param l: label of a study
        @rtype: C{List}
        @return: C{List} of list of nodes <postpro>
        """
        scripts, labels, nodes, args = [], [], [], []

        for node in self.getStudyNode(l).getElementsByTagName("postpro"):
            if str(node.attributes["status"].value) == 'on':
                scripts.append(True)
            else:
                scripts.append(False)

            labels.append(str(node.attributes["label"].value))
            nodes.append(node)
            try:
                args.append(str(node.attributes["args"].value))
            except:
                args.append("")

        return scripts, labels, nodes, args


    #---------------------------------------------------------------------------

    def getSubplots(self, studyLabel):
        return self.getStudyNode(studyLabel).getElementsByTagName("subplot")

    #---------------------------------------------------------------------------

    def getFigures(self, studyLabel):
        return self.getStudyNode(studyLabel).getElementsByTagName("figure")

    #---------------------------------------------------------------------------

    def getPltCommands(self, node):
        """
        """
        cmd = []
        for n in node.getElementsByTagName("plt_command"):
            if n.nodeType == minidom.Node.ELEMENT_NODE and n.childNodes:
                if n.tagName == "plt_command":
                    cmd.append(n.childNodes[0].data)
        return cmd

    #---------------------------------------------------------------------------

    def getAttributes(self, node):
        """
        Return a dictionary with attributes and value of a node.
        """
        d = {}
        for k in node.attributes.keys():
            d[k] = node.attributes[k].value
        return d

    #---------------------------------------------------------------------------

    def getAttribute(self, node, k, default = None):
        """
        Return a value of an attribute.
        """
        if k in node.attributes.keys():
            return node.attributes[k].value
        else:
            if default is None:
                raise ValueError("Error: attribute %s is mandatory!" % k)
            else:
                return default

    #---------------------------------------------------------------------------

    def getAttributeTuple(self, node, k, default = None):
        """
        Return a value of an attribute.
        """
        if k in node.attributes.keys():
            n = node.attributes[k].value
            return tuple(float(s) for s in n[1:-1].split(','))
        else:
            if default is None:
                raise ValueError("Error: attribute %s is mandatory!" % k)
            else:
                return default


#-------------------------------------------------------------------------------
