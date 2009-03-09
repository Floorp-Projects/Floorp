/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2007                *
 * by the Xiph.Org Foundation and contributors http://www.xiph.org/ *
 *                                                                  *
 ********************************************************************

  function:
    last mod: $Id: huffdec.c 15431 2008-10-21 05:04:02Z giles $

 ********************************************************************/

#include <stdlib.h>
#include <ogg/ogg.h>
#include "huffdec.h"
#include "decint.h"


/*The ANSI offsetof macro is broken on some platforms (e.g., older DECs).*/
#define _ogg_offsetof(_type,_field)\
 ((size_t)((char *)&((_type *)0)->_field-(char *)0))

/*These two functions are really part of the bitpack.c module, but
  they are only used here. Declaring local static versions so they
  can be inlined saves considerable function call overhead.*/

/*Read in bits without advancing the bitptr.
  Here we assume 0<=_bits&&_bits<=32.*/
static int theorapackB_look(oggpack_buffer *_b,int _bits,long *_ret){
  long ret;
  long m;
  long d;
  m=32-_bits;
  _bits+=_b->endbit;
  d=_b->storage-_b->endbyte;
  if(d<=4){
    /*Not the main path.*/
    if(d<=0){
      *_ret=0L;
      return -(_bits>d*8);
    }
    /*If we have some bits left, but not enough, return the ones we have.*/
    if(d*8<_bits)_bits=d*8;
  }
  ret=_b->ptr[0]<<24+_b->endbit;
  if(_bits>8){
    ret|=_b->ptr[1]<<16+_b->endbit;
    if(_bits>16){
      ret|=_b->ptr[2]<<8+_b->endbit;
      if(_bits>24){
        ret|=_b->ptr[3]<<_b->endbit;
        if(_bits>32)ret|=_b->ptr[4]>>8-_b->endbit;
      }
    }
  }
  *_ret=((ret&0xFFFFFFFF)>>(m>>1))>>(m+1>>1);
  return 0;
}

/*advance the bitptr*/
static void theorapackB_adv(oggpack_buffer *_b,int _bits){
  _bits+=_b->endbit;
  _b->ptr+=_bits>>3;
  _b->endbyte+=_bits>>3;
  _b->endbit=_bits&7;
}


/*The log_2 of the size of a lookup table is allowed to grow to relative to
   the number of unique nodes it contains.
  E.g., if OC_HUFF_SLUSH is 2, then at most 75% of the space in the tree is
   wasted (each node will have an amortized cost of at most 20 bytes when using
   4-byte pointers).
  Larger numbers can decode tokens with fewer read operations, while smaller
   numbers may save more space (requiring as little as 8 bytes amortized per
   node, though there will be more nodes).
  With a sample file:
  32233473 read calls are required when no tree collapsing is done (100.0%).
  19269269 read calls are required when OC_HUFF_SLUSH is 0 (59.8%).
  11144969 read calls are required when OC_HUFF_SLUSH is 1 (34.6%).
  10538563 read calls are required when OC_HUFF_SLUSH is 2 (32.7%).
  10192578 read calls are required when OC_HUFF_SLUSH is 3 (31.6%).
  Since a value of 1 gets us the vast majority of the speed-up with only a
   small amount of wasted memory, this is what we use.*/
#define OC_HUFF_SLUSH (1)


/*Allocates a Huffman tree node that represents a subtree of depth _nbits.
  _nbits: The depth of the subtree.
          If this is 0, the node is a leaf node.
          Otherwise 1<<_nbits pointers are allocated for children.
  Return: The newly allocated and fully initialized Huffman tree node.*/
static oc_huff_node *oc_huff_node_alloc(int _nbits){
  oc_huff_node *ret;
  size_t        size;
  size=_ogg_offsetof(oc_huff_node,nodes);
  if(_nbits>0)size+=sizeof(oc_huff_node *)*(1<<_nbits);
  ret=_ogg_calloc(1,size);
  ret->nbits=(unsigned char)_nbits;
  return ret;
}

/*Frees a Huffman tree node allocated with oc_huf_node_alloc.
  _node: The node to free.
         This may be NULL.*/
static void oc_huff_node_free(oc_huff_node *_node){
  _ogg_free(_node);
}

/*Frees the memory used by a Huffman tree.
  _node: The Huffman tree to free.
         This may be NULL.*/
static void oc_huff_tree_free(oc_huff_node *_node){
  if(_node==NULL)return;
  if(_node->nbits){
    int nchildren;
    int i;
    int inext;
    nchildren=1<<_node->nbits;
    for(i=0;i<nchildren;i=inext){
      inext=i+(_node->nodes[i]!=NULL?1<<_node->nbits-_node->nodes[i]->depth:1);
      oc_huff_tree_free(_node->nodes[i]);
    }
  }
  oc_huff_node_free(_node);
}

/*Unpacks a sub-tree from the given buffer.
  _opb:    The buffer to unpack from.
  _binode: The location to store a pointer to the sub-tree in.
  _depth:  The current depth of the tree.
           This is used to prevent infinite recursion.
  Return: 0 on success, or a negative value on error.*/
static int oc_huff_tree_unpack(oggpack_buffer *_opb,
 oc_huff_node **_binode,int _depth){
  oc_huff_node *binode;
  long          bits;
  /*Prevent infinite recursion.*/
  if(++_depth>32)return TH_EBADHEADER;
  if(theorapackB_read1(_opb,&bits)<0)return TH_EBADHEADER;
  /*Read an internal node:*/
  if(!bits){
    int ret;
    binode=oc_huff_node_alloc(1);
    binode->depth=(unsigned char)(_depth>1);
    ret=oc_huff_tree_unpack(_opb,binode->nodes,_depth);
    if(ret>=0)ret=oc_huff_tree_unpack(_opb,binode->nodes+1,_depth);
    if(ret<0){
      oc_huff_tree_free(binode);
      *_binode=NULL;
      return ret;
    }
  }
  /*Read a leaf node:*/
  else{
    if(theorapackB_read(_opb,OC_NDCT_TOKEN_BITS,&bits)<0)return TH_EBADHEADER;
    binode=oc_huff_node_alloc(0);
    binode->depth=(unsigned char)(_depth>1);
    binode->token=(unsigned char)bits;
  }
  *_binode=binode;
  return 0;
}

/*Finds the depth of shortest branch of the given sub-tree.
  The tree must be binary.
  _binode: The root of the given sub-tree.
           _binode->nbits must be 0 or 1.
  Return: The smallest depth of a leaf node in this sub-tree.
          0 indicates this sub-tree is a leaf node.*/
static int oc_huff_tree_mindepth(oc_huff_node *_binode){
  int depth0;
  int depth1;
  if(_binode->nbits==0)return 0;
  depth0=oc_huff_tree_mindepth(_binode->nodes[0]);
  depth1=oc_huff_tree_mindepth(_binode->nodes[1]);
  return OC_MINI(depth0,depth1)+1;
}

/*Finds the number of internal nodes at a given depth, plus the number of
   leaves at that depth or shallower.
  The tree must be binary.
  _binode: The root of the given sub-tree.
           _binode->nbits must be 0 or 1.
  Return: The number of entries that would be contained in a jump table of the
           given depth.*/
static int oc_huff_tree_occupancy(oc_huff_node *_binode,int _depth){
  if(_binode->nbits==0||_depth<=0)return 1;
  else{
    return oc_huff_tree_occupancy(_binode->nodes[0],_depth-1)+
     oc_huff_tree_occupancy(_binode->nodes[1],_depth-1);
  }
}

static oc_huff_node *oc_huff_tree_collapse(oc_huff_node *_binode);

/*Fills the given nodes table with all the children in the sub-tree at the
   given depth.
  The nodes in the sub-tree with a depth less than that stored in the table
   are freed.
  The sub-tree must be binary and complete up until the given depth.
  _nodes:  The nodes table to fill.
  _binode: The root of the sub-tree to fill it with.
           _binode->nbits must be 0 or 1.
  _level:  The current level in the table.
           0 indicates that the current node should be stored, regardless of
            whether it is a leaf node or an internal node.
  _depth:  The depth of the nodes to fill the table with, relative to their
            parent.*/
static void oc_huff_node_fill(oc_huff_node **_nodes,
 oc_huff_node *_binode,int _level,int _depth){
  if(_level<=0||_binode->nbits==0){
    int i;
    _binode->depth=(unsigned char)(_depth-_level);
    _nodes[0]=oc_huff_tree_collapse(_binode);
    for(i=1;i<1<<_level;i++)_nodes[i]=_nodes[0];
  }
  else{
    _level--;
    oc_huff_node_fill(_nodes,_binode->nodes[0],_level,_depth);
    oc_huff_node_fill(_nodes+(1<<_level),_binode->nodes[1],_level,_depth);
    oc_huff_node_free(_binode);
  }
}

/*Finds the largest complete sub-tree rooted at the current node and collapses
   it into a single node.
  This procedure is then applied recursively to all the children of that node.
  _binode: The root of the sub-tree to collapse.
           _binode->nbits must be 0 or 1.
  Return: The new root of the collapsed sub-tree.*/
static oc_huff_node *oc_huff_tree_collapse(oc_huff_node *_binode){
  oc_huff_node *root;
  int           mindepth;
  int           depth;
  int           loccupancy;
  int           occupancy;
  depth=mindepth=oc_huff_tree_mindepth(_binode);
  occupancy=1<<mindepth;
  do{
    loccupancy=occupancy;
    occupancy=oc_huff_tree_occupancy(_binode,++depth);
  }
  while(occupancy>loccupancy&&occupancy>=1<<OC_MAXI(depth-OC_HUFF_SLUSH,0));
  depth--;
  if(depth<=1)return _binode;
  root=oc_huff_node_alloc(depth);
  root->depth=_binode->depth;
  oc_huff_node_fill(root->nodes,_binode,depth,depth);
  return root;
}

/*Makes a copy of the given Huffman tree.
  _node: The Huffman tree to copy.
  Return: The copy of the Huffman tree.*/
static oc_huff_node *oc_huff_tree_copy(const oc_huff_node *_node){
  oc_huff_node *ret;
  ret=oc_huff_node_alloc(_node->nbits);
  ret->depth=_node->depth;
  if(_node->nbits){
    int nchildren;
    int i;
    int inext;
    nchildren=1<<_node->nbits;
    for(i=0;i<nchildren;){
      ret->nodes[i]=oc_huff_tree_copy(_node->nodes[i]);
      inext=i+(1<<_node->nbits-ret->nodes[i]->depth);
      while(++i<inext)ret->nodes[i]=ret->nodes[i-1];
    }
  }
  else ret->token=_node->token;
  return ret;
}

/*Unpacks a set of Huffman trees, and reduces them to a collapsed
   representation.
  _opb:   The buffer to unpack the trees from.
  _nodes: The table to fill with the Huffman trees.
  Return: 0 on success, or a negative value on error.*/
int oc_huff_trees_unpack(oggpack_buffer *_opb,
 oc_huff_node *_nodes[TH_NHUFFMAN_TABLES]){
  int i;
  for(i=0;i<TH_NHUFFMAN_TABLES;i++){
    int ret;
    ret=oc_huff_tree_unpack(_opb,_nodes+i,0);
    if(ret<0)return ret;
    _nodes[i]=oc_huff_tree_collapse(_nodes[i]);
  }
  return 0;
}

/*Makes a copy of the given set of Huffman trees.
  _dst: The array to store the copy in.
  _src: The array of trees to copy.*/
void oc_huff_trees_copy(oc_huff_node *_dst[TH_NHUFFMAN_TABLES],
 const oc_huff_node *const _src[TH_NHUFFMAN_TABLES]){
  int i;
  for(i=0;i<TH_NHUFFMAN_TABLES;i++)_dst[i]=oc_huff_tree_copy(_src[i]);
}

/*Frees the memory used by a set of Huffman trees.
  _nodes: The array of trees to free.*/
void oc_huff_trees_clear(oc_huff_node *_nodes[TH_NHUFFMAN_TABLES]){
  int i;
  for(i=0;i<TH_NHUFFMAN_TABLES;i++)oc_huff_tree_free(_nodes[i]);
}

/*Unpacks a single token using the given Huffman tree.
  _opb:  The buffer to unpack the token from.
  _node: The tree to unpack the token with.
  Return: The token value.*/
int oc_huff_token_decode(oggpack_buffer *_opb,const oc_huff_node *_node){
  long bits;
  while(_node->nbits!=0){
    theorapackB_look(_opb,_node->nbits,&bits);
    _node=_node->nodes[bits];
    theorapackB_adv(_opb,_node->depth);
  }
  return _node->token;
}
