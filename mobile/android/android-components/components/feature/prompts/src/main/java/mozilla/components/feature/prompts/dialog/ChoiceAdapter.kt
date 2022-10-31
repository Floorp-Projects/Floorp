/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.dialog

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.CheckedTextView
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.concept.engine.prompt.Choice
import mozilla.components.feature.prompts.R

/**
 * RecyclerView adapter for displaying choice items.
 */
internal class ChoiceAdapter(
    private val fragment: ChoiceDialogFragment,
    private val inflater: LayoutInflater,
) : RecyclerView.Adapter<RecyclerView.ViewHolder>() {

    companion object {
        internal const val TYPE_MULTIPLE = 1
        internal const val TYPE_SINGLE = 2
        internal const val TYPE_GROUP = 3
        internal const val TYPE_MENU = 4
        internal const val TYPE_MENU_SEPARATOR = 5
    }

    private val choices = mutableListOf<Choice>()

    init {
        addItems(fragment.choices)
    }

    override fun getItemViewType(position: Int): Int {
        val item = choices[position]
        return when {
            fragment.isSingleChoice and item.isGroupType -> TYPE_GROUP
            fragment.isSingleChoice -> TYPE_SINGLE
            fragment.isMenuChoice -> if (item.isASeparator) TYPE_MENU_SEPARATOR else TYPE_MENU
            item.isGroupType -> TYPE_GROUP
            else -> TYPE_MULTIPLE
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, type: Int): RecyclerView.ViewHolder {
        val layoutId = getLayoutId(type)
        val view = inflater.inflate(layoutId, parent, false)

        return when (type) {
            TYPE_GROUP -> GroupViewHolder(view)

            TYPE_MENU -> MenuViewHolder(view)

            TYPE_MENU_SEPARATOR -> MenuSeparatorViewHolder(view)

            TYPE_SINGLE -> SingleViewHolder(view)

            TYPE_MULTIPLE -> MultipleViewHolder(view)

            else -> throw IllegalArgumentException(" $type is not a valid layout type")
        }
    }

    override fun getItemCount(): Int = choices.size

    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        val choice = choices[position]
        when (holder) {
            is MenuSeparatorViewHolder -> return

            is GroupViewHolder -> {
                holder.bind(choice)
            }

            is SingleViewHolder -> {
                holder.bind(choice, fragment)
            }

            is MultipleViewHolder -> {
                holder.bind(choice, fragment)
            }

            is MenuViewHolder -> {
                holder.bind(choice, fragment)
            }
        }
    }

    private fun getLayoutId(itemType: Int): Int {
        return when (itemType) {
            TYPE_GROUP -> R.layout.mozac_feature_choice_group_item
            TYPE_MULTIPLE -> R.layout.mozac_feature_multiple_choice_item
            TYPE_SINGLE -> R.layout.mozac_feature_single_choice_item
            TYPE_MENU -> R.layout.mozac_feature_menu_choice_item
            TYPE_MENU_SEPARATOR -> R.layout.mozac_feature_menu_separator_choice_item
            else -> throw IllegalArgumentException(" $itemType is not a valid layout dialog type")
        }
    }

    /**
     * View holder for a single choice item.
     */
    internal class SingleViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        internal val labelView = itemView.findViewById<CheckedTextView>(R.id.labelView)

        fun bind(choice: Choice, fragment: ChoiceDialogFragment) {
            labelView.choice = choice
            labelView.isChecked = choice.selected

            if (choice.enable) {
                itemView.setOnClickListener {
                    val actualChoice = labelView.choice
                    fragment.onSelect(actualChoice)
                    labelView.toggle()
                }
            } else {
                itemView.isClickable = false
            }
        }
    }

    /**
     * View holder for a Multiple choice item.
     */
    internal class MultipleViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        internal val labelView = itemView.findViewById<CheckedTextView>(R.id.labelView)

        fun bind(choice: Choice, fragment: ChoiceDialogFragment) {
            labelView.choice = choice
            labelView.isChecked = choice.selected

            if (choice.enable) {
                itemView.setOnClickListener {
                    val actualChoice = labelView.choice
                    with(fragment.mapSelectChoice) {
                        if (actualChoice in this) {
                            this -= actualChoice
                        } else {
                            this[actualChoice] = actualChoice
                        }
                    }
                    labelView.toggle()
                }
            } else {
                itemView.isClickable = false
            }
        }
    }

    /**
     * View holder for a Menu choice item.
     */
    internal class MenuViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        internal val labelView = itemView.findViewById<TextView>(R.id.labelView)

        fun bind(choice: Choice, fragment: ChoiceDialogFragment) {
            labelView.choice = choice

            if (choice.enable) {
                itemView.setOnClickListener {
                    val actualChoice = labelView.choice
                    fragment.onSelect(actualChoice)
                }
            } else {
                itemView.isClickable = false
            }
        }
    }

    /**
     * View holder for a menu separator choice item.
     */
    internal class MenuSeparatorViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView)

    /**
     * View holder for a group choice item.
     */
    internal class GroupViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        internal val labelView = itemView.findViewById<TextView>(R.id.labelView)

        fun bind(choice: Choice) {
            labelView.choice = choice
            labelView.isEnabled = false
        }
    }

    private fun addItems(items: Array<Choice>, indent: String? = null) {
        for (choice in items) {
            if (indent != null && !choice.isGroupType) {
                choice.label = indent + choice.label
            }

            choices.add(choice)

            if (choice.isGroupType) {
                val newIndent = if (indent != null) indent + '\t' else "\t"
                addItems(requireNotNull(choice.children), newIndent)
            }

            if (choice.selected) {
                fragment.mapSelectChoice[choice] = choice
            }
        }
    }
}

internal var TextView.choice: Choice
    get() = tag as Choice
    set(value) {
        this.text = value.label
        this.isEnabled = value.enable
        tag = value
    }
