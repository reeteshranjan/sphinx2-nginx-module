sphinx2-nginx-module
====================

    Nginx upstream module for Sphinx 2.x search daemon

    *This module is not distributed with the Nginx source.* See the installation
    instructions.

Status

    Alpha [Work in progress]

Version

    0.2

Synopsis

    # Search
    # /search?offset=0&nres=20&match=all
    #         &ranker=proxbm25&rankexpr=
    #         &sort=relevance&sortby=myattr
    #         &keywords=Anna+Hazare
    #         &index=myidx
    #         &filters=a1,in,range,10,20;a2,ex,range,10,20;a3,in,frange,10.00,20.00
    #         &group=day,attr,@group desc,attr2
    #         &maxres=1000
    #         &geo=latattr,lonattr,10.00,10.00
    #         &idxweights=myidx:50;othidx:10
    #         &fldweights=content:100;title:0
    #         &format=json
    location /search {
        set_unescape_uri    $sphinx2_command   "search";
        set_unescape_uri    $sphx_offset       $arg_offset;
        set_unescape_uri    $sphx_numresults   $arg_nres;
        set_unescape_uri    $sphx_matchmode    $arg_match;
        set_unescape_uri    $sphx_ranker       $arg_ranker;
        set_unescape_uri    $sphx_rankexpr     $arg_rankexpr;
        set_unescape_uri    $sphx_sortmode     $arg_sort;
        set_unescape_uri    $sphx_sortby       $arg_sortby;
        set_unescape_uri    $sphx_keywords     $arg_keywords;
        set_unescape_uri    $sphx_index        $arg_index;
        set_unescape_uri    $sphx_filters      $arg_filters;
        set_unescape_uri    $sphx_group        $arg_group;
        set_unescape_uri    $sphx_maxmatches   $arg_maxres;
        set_unescape_uri    $sphx_geo          $arg_geo;
        set_unescape_uri    $sphx_indexweights $arg_idxweights;
        set_unescape_uri    $sphx_fieldweights $arg_fldweights;
        set_unescape_uri    $sphx_outputtype   $arg_format;
        set_unescape_uri    $sphx_docs         "";
        set_unescape_uri    $sphx_excerpt_opts "";
        sphinx2_pass        127.0.0.1:9312;
    }

    # Excerpt
    # /excerpt?keywords=Anna+Hazare&index=myidx
    #         &opts=before_match:<b>,after_match:</b>,chunk_separator: ...,
    #             limit:256,limit_passages:0,limit_words:0,around:5,
    #             exact_phrase:0,single_passage:0,use_boundaries:0,
    #             weight_order:0,query_mode:0,force_all_words:0,
    #             start_passage_id:1,load_files:0,html_strip_mode:index,
    #             allow_empty:0,passage_boundary:none,emit_zones:0,
    #             load_files_scattered:0
    #         &docs=<url-escaped doc strings separated by ;>
    location /excerpt {
        set_unescape_uri    $sphinx2_command   "excerpt";
        set_unescape_uri    $sphx_offset       "";
        set_unescape_uri    $sphx_numresults   "";
        set_unescape_uri    $sphx_matchmode    "";
        set_unescape_uri    $sphx_ranker       "";
        set_unescape_uri    $sphx_rankexpr     "";
        set_unescape_uri    $sphx_sortmode     "";
        set_unescape_uri    $sphx_sortby       "";
        set_unescape_uri    $sphx_keywords     $arg_keywords;
        set_unescape_uri    $sphx_index        $arg_index;
        set_unescape_uri    $sphx_filters      "";
        set_unescape_uri    $sphx_group        "";
        set_unescape_uri    $sphx_maxmatches   "";
        set_unescape_uri    $sphx_geo          "";
        set_unescape_uri    $sphx_indexweights "";
        set_unescape_uri    $sphx_fieldweights "";
        set_unescape_uri    $sphx_outputtype   "";
        set_unescape_uri    $sphx_docs         $arg_docs;
        set_unescape_uri    $sphx_excerpt_opts $arg_opts;
        sphinx2_pass        127.0.0.1:9312;
    }

Description

    This is an Nginx upstream module that makes nginx talk to a Sphinx
    (<http://sphinxsearch.com/>) 2.0.8 server in a non-blocking way.

    Following features are supported as of now.
    1  Search
    2  Excerpt

    The module outputs the raw TCP response from searchd minus the 
    handshake and header bytes.

Compatibility

    Verified with:
    
    *   nginx-1.4.3
    *   sphinx-2.0.8

Limitations

    *   Portions of code are 64-bit specific.
    *   Portions of code are Little Endian specific.
    *   Some parameters for Sphinx searchd request have been given default
        values and user cannot control them through a query string param. The
        details are as follows:
        <TODO>

Installation Instructions

    *   Get nginx source from nginx.org (see "Comptability" above)

    *   Download a release tarball of sphinx2-nginx-module from
        https://github.com/reeteshranjan/sphinx2-nginx-module/releases

    *   Include --add-module=/path/to/sphinx2-nginx-module directive in the
        configure command for building nginx source.

    *   make & make install (assuming you have permission for install folders)

    *   Use sample nginx conf file section above to modify your nginx conf.

Query Parameters Description

    <TODO>
